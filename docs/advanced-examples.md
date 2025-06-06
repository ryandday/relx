---
layout: default
title: Advanced Examples
nav_order: 6
---

# Advanced Examples

This guide provides comprehensive examples of using relx for complex query scenarios and real-world applications.

## Table of Contents

- [Schema Setup](#schema-setup)
- [Complex Joins](#complex-joins)
- [Aggregate Functions](#aggregate-functions)
- [Subqueries](#subqueries)
- [Case Expressions](#case-expressions)
- [Window Functions](#window-functions)
- [Transaction Management](#transaction-management)
- [Real-World Application Examples](#real-world-application-examples)

## Schema Setup

We'll use a comprehensive schema for our examples:

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/postgresql.hpp>
#include <string>
#include <optional>

// Users table
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "age", int> age;
    relx::column<Users, "is_active", bool> is_active;
    relx::column<Users, "department_id", int> department_id;
    relx::column<Users, "bio", std::optional<std::string>> bio;
    relx::column<Users, "created_at", std::string> created_at;
    
    relx::unique_constraint<&Users::email> unique_email;
    relx::foreign_key<&Users::department_id, &Departments::id> dept_fk;
};

// Posts table
struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::column<Posts, "id", int, relx::primary_key> id;
    relx::column<Posts, "user_id", int> user_id;
    relx::column<Posts, "title", std::string> title;
    relx::column<Posts, "content", std::string> content;
    relx::column<Posts, "views", int> views;
    relx::column<Posts, "created_at", std::string> created_at;
    relx::column<Posts, "published", bool> published;
    
    relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
    relx::index<&Posts::user_id> user_idx;
    relx::index<&Posts::published> published_idx;
};

// Comments table
struct Comments {
    static constexpr auto table_name = "comments";
    
    relx::column<Comments, "id", int, relx::primary_key> id;
    relx::column<Comments, "post_id", int> post_id;
    relx::column<Comments, "user_id", int> user_id;
    relx::column<Comments, "content", std::string> content;
    relx::column<Comments, "created_at", std::string> created_at;
    relx::column<Comments, "approved", bool> approved;
    
    relx::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    relx::foreign_key<&Comments::user_id, &Users::id> user_fk;
};

// Departments table
struct Departments {
    static constexpr auto table_name = "departments";
    
    relx::column<Departments, "id", int, relx::primary_key> id;
    relx::column<Departments, "name", std::string> name;
    relx::column<Departments, "budget", double> budget;
    relx::column<Departments, "manager_id", std::optional<int>> manager_id;
    
    relx::foreign_key<&Departments::manager_id, &Users::id> manager_fk;
};

// Tags table for many-to-many relationship
struct Tags {
    static constexpr auto table_name = "tags";
    
    relx::column<Tags, "id", int, relx::primary_key> id;
    relx::column<Tags, "name", std::string> name;
    
    relx::unique_constraint<&Tags::name> unique_name;
};

// Post-Tag junction table
struct PostTags {
    static constexpr auto table_name = "post_tags";
    
    relx::column<PostTags, "post_id", int> post_id;
    relx::column<PostTags, "tag_id", int> tag_id;
    
    relx::foreign_key<&PostTags::post_id, &Posts::id> post_fk;
    relx::foreign_key<&PostTags::tag_id, &Tags::id> tag_fk;
    relx::primary_key<&PostTags::post_id, &PostTags::tag_id> composite_pk;
};
```

## Complex Joins

### Multi-Table Joins with Filtering

Finding users, their posts, and approved comments:

```cpp
Users u;
Posts p;
Comments c;

struct PostWithCommentsDTO {
    std::string user_name;
    std::string post_title;
    int views;
    std::string comment_content;
    std::string comment_date;
};

auto query = relx::select(
    u.name,
    p.title,
    p.views,
    c.content,
    c.created_at
).from(u)
 .join(p, relx::on(u.id == p.user_id))
 .join(c, relx::on(p.id == c.post_id))
 .where(p.published == true && c.approved == true)
 .order_by(p.views.desc(), c.created_at);

auto result = conn.execute<PostWithCommentsDTO>(query);
```

### Left Joins with Null Handling

Finding all users and their post counts (including users with no posts):

```cpp
Users u;
Posts p;

struct UserPostStatsDTO {
    int user_id;
    std::string user_name;
    std::optional<int> post_count;
    std::optional<int> total_views;
};

auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(relx::count(p.id), "post_count"),
    relx::as(relx::sum(p.views), "total_views")
).from(u)
 .left_join(p, relx::on(u.id == p.user_id && p.published == true))
 .group_by(u.id, u.name)
 .order_by(relx::coalesce(relx::count(p.id), 0).desc());

auto result = conn.execute<UserPostStatsDTO>(query);
```

### Self-Joins for Hierarchical Data

Finding department managers and their departments:

```cpp
Users u1, u2;  // Aliases for self-join
Departments d;

struct ManagerInfoDTO {
    std::string department_name;
    std::optional<std::string> manager_name;
    std::optional<std::string> manager_email;
    double budget;
};

auto query = relx::select(
    d.name,
    u1.name,
    u1.email,
    d.budget
).from(d)
 .left_join(u1, relx::on(d.manager_id == u1.id))
 .order_by(d.name);

auto result = conn.execute<ManagerInfoDTO>(query);
```

## Aggregate Functions

### Complex Aggregations with Multiple Grouping

Department statistics with user and post metrics:

```cpp
Users u;
Posts p;
Departments d;

struct DepartmentStatsDTO {
    std::string department_name;
    int active_users;
    int total_posts;
    int total_views;
    double avg_user_age;
    double posts_per_user;
};

auto query = relx::select_expr(
    d.name,
    relx::as(relx::count(relx::distinct(u.id)), "active_users"),
    relx::as(relx::count(p.id), "total_posts"),
    relx::as(relx::sum(p.views), "total_views"),
    relx::as(relx::avg(u.age), "avg_user_age"),
    relx::as(
        relx::case_()
            .when(relx::count(relx::distinct(u.id)) > 0,
                  relx::count(p.id) / relx::count(relx::distinct(u.id)))
            .else_(0)
            .build(),
        "posts_per_user"
    )
).from(d)
 .join(u, relx::on(d.id == u.department_id && u.is_active == true))
 .left_join(p, relx::on(u.id == p.user_id && p.published == true))
 .group_by(d.id, d.name)
 .having(relx::count(relx::distinct(u.id)) > 0)
 .order_by(relx::count(p.id).desc());

auto result = conn.execute<DepartmentStatsDTO>(query);
```

### Window Functions for Advanced Analytics

Ranking posts by views within each department:

```cpp
Users u;
Posts p;
Departments d;

struct PostRankingDTO {
    std::string department_name;
    std::string user_name;
    std::string post_title;
    int views;
    int rank_in_department;
    int total_department_posts;
};

auto query = relx::select_expr(
    d.name,
    u.name,
    p.title,
    p.views,
    relx::as(
        relx::rank().over(
            relx::partition_by(d.id)
            .order_by(p.views.desc())
        ),
        "rank_in_department"
    ),
    relx::as(
        relx::count().over(relx::partition_by(d.id)),
        "total_department_posts"
    )
).from(p)
 .join(u, relx::on(p.user_id == u.id))
 .join(d, relx::on(u.department_id == d.id))
 .where(p.published == true)
 .order_by(d.name, relx::rank().over(
     relx::partition_by(d.id).order_by(p.views.desc())
 ));

auto result = conn.execute<PostRankingDTO>(query);
```

## Subqueries

### Correlated Subqueries

Finding users who have more posts than the average in their department:

```cpp
Users u1, u2;
Posts p1, p2;
Departments d;

struct ProductiveUserDTO {
    std::string user_name;
    std::string department_name;
    int user_post_count;
    double dept_avg_posts;
};

auto dept_avg_subquery = relx::select_expr(
    relx::avg(relx::count(p2.id))
).from(u2)
 .join(p2, relx::on(u2.id == p2.user_id))
 .where(u2.department_id == u1.department_id)
 .group_by(u2.id);

auto query = relx::select_expr(
    u1.name,
    d.name,
    relx::as(relx::count(p1.id), "user_post_count"),
    relx::as(dept_avg_subquery, "dept_avg_posts")
).from(u1)
 .join(d, relx::on(u1.department_id == d.id))
 .left_join(p1, relx::on(u1.id == p1.user_id && p1.published == true))
 .group_by(u1.id, u1.name, d.name)
 .having(relx::count(p1.id) > dept_avg_subquery)
 .order_by(relx::count(p1.id).desc());

auto result = conn.execute<ProductiveUserDTO>(query);
```

### EXISTS Subqueries

Finding posts that have at least one approved comment:

```cpp
Posts p;
Comments c;

struct PostWithCommentsDTO {
    int post_id;
    std::string title;
    int views;
    std::string created_at;
};

auto has_comments = relx::exists(
    relx::select(c.id)
        .from(c)
        .where(c.post_id == p.id && c.approved == true)
);

auto query = relx::select(p.id, p.title, p.views, p.created_at)
    .from(p)
    .where(p.published == true && has_comments)
    .order_by(p.views.desc());

auto result = conn.execute<PostWithCommentsDTO>(query);
```

## Case Expressions

### Complex Conditional Logic

Categorizing users based on their activity level:

```cpp
Users u;
Posts p;

struct UserActivityDTO {
    std::string user_name;
    int post_count;
    std::string activity_level;
    std::string engagement_tier;
};

auto query = relx::select_expr(
    u.name,
    relx::as(relx::count(p.id), "post_count"),
    relx::as(
        relx::case_()
            .when(relx::count(p.id) >= 10, "Very Active")
            .when(relx::count(p.id) >= 5, "Active")
            .when(relx::count(p.id) >= 1, "Occasional")
            .else_("Inactive")
            .build(),
        "activity_level"
    ),
    relx::as(
        relx::case_()
            .when(relx::sum(p.views) >= 1000, "Influencer")
            .when(relx::sum(p.views) >= 100, "Regular")
            .when(relx::sum(p.views) > 0, "Beginner")
            .else_("No Impact")
            .build(),
        "engagement_tier"
    )
).from(u)
 .left_join(p, relx::on(u.id == p.user_id && p.published == true))
 .where(u.is_active == true)
 .group_by(u.id, u.name)
 .order_by(relx::count(p.id).desc());

auto result = conn.execute<UserActivityDTO>(query);
```

## Transaction Management

### Complex Transaction with Multiple Operations

Managing a complete blog post creation workflow:

```cpp
relx::ConnectionResult<int> create_blog_post_with_tags(
    relx::PostgreSQLConnection& conn,
    const std::string& title,
    const std::string& content,
    int user_id,
    const std::vector<std::string>& tag_names
) {
    auto transaction = conn.begin_transaction();
    
    try {
        Posts posts;
        Tags tags;
        PostTags post_tags;
        
        // 1. Insert the post
        auto post_insert = relx::insert_into(posts)
            .columns(posts.user_id, posts.title, posts.content, posts.published)
            .values(user_id, title, content, true)
            .returning(posts.id);
        
        auto post_result = conn.execute(post_insert);
        relx::throw_if_failed(post_result);
        
        int post_id = (*post_result)[0].get<int>("id").value();
        
        // 2. Insert or get existing tags
        for (const auto& tag_name : tag_names) {
            // Try to insert tag (will fail if already exists)
            auto tag_insert = relx::insert_into(tags)
                .columns(tags.name)
                .values(tag_name)
                .on_conflict(tags.name)
                .do_nothing()
                .returning(tags.id);
            
            auto tag_result = conn.execute(tag_insert);
            
            int tag_id;
            if (tag_result && !tag_result->empty()) {
                // New tag was inserted
                tag_id = (*tag_result)[0].get<int>("id").value();
            } else {
                // Tag already exists, get its ID
                auto tag_select = relx::select(tags.id)
                    .from(tags)
                    .where(tags.name == tag_name);
                
                auto existing_tag = relx::value_or_throw(conn.execute(tag_select));
                tag_id = existing_tag[0].get<int>("id").value();
            }
            
            // 3. Link post to tag
            auto post_tag_insert = relx::insert_into(post_tags)
                .columns(post_tags.post_id, post_tags.tag_id)
                .values(post_id, tag_id);
            
            relx::throw_if_failed(conn.execute(post_tag_insert));
        }
        
        // 4. Update user's post count (hypothetical trigger alternative)
        auto update_user = relx::update(posts.user_table)
            .set(relx::set(posts.user_table.post_count, 
                          posts.user_table.post_count + 1))
            .where(posts.user_table.id == user_id);
        
        relx::throw_if_failed(conn.execute(update_user));
        
        // Commit transaction
        relx::throw_if_failed(transaction.commit());
        
        return post_id;
        
    } catch (const relx::RelxException& e) {
        auto rollback_result = transaction.rollback();
        if (!rollback_result) {
            std::println("Rollback failed: {}", rollback_result.error().message);
        }
        
        return std::unexpected(QueryError{
            .message = e.what(),
            .error_code = 0,
            .query_string = "create_blog_post_transaction"
        });
    }
}
```

## Real-World Application Examples

### Blog Analytics Dashboard

Comprehensive blog analytics with multiple metrics:

```cpp
struct BlogAnalyticsDTO {
    // Overall stats
    int total_users;
    int active_users;
    int total_posts;
    int published_posts;
    int total_comments;
    int approved_comments;
    
    // Engagement metrics
    double avg_posts_per_user;
    double avg_comments_per_post;
    double approval_rate;
    
    // Content metrics
    int total_views;
    double avg_views_per_post;
    std::string most_popular_post;
    std::string most_active_user;
};

relx::ConnectionResult<BlogAnalyticsDTO> get_blog_analytics(
    relx::PostgreSQLConnection& conn
) {
    try {
        Users u;
        Posts p;
        Comments c;
        
        // Get overall statistics
        auto stats_query = relx::select_expr(
            relx::as(relx::count(relx::distinct(u.id)), "total_users"),
            relx::as(relx::count(relx::distinct(
                relx::case_()
                    .when(u.is_active == true, u.id)
                    .else_(relx::null<int>())
                    .build()
            )), "active_users"),
            relx::as(relx::count(p.id), "total_posts"),
            relx::as(relx::count(
                relx::case_()
                    .when(p.published == true, p.id)
                    .else_(relx::null<int>())
                    .build()
            ), "published_posts"),
            relx::as(relx::count(c.id), "total_comments"),
            relx::as(relx::count(
                relx::case_()
                    .when(c.approved == true, c.id)
                    .else_(relx::null<int>())
                    .build()
            ), "approved_comments"),
            relx::as(relx::sum(p.views), "total_views")
        ).from(u)
         .left_join(p, relx::on(u.id == p.user_id))
         .left_join(c, relx::on(p.id == c.post_id));
        
        auto stats_result = relx::value_or_throw(conn.execute(stats_query));
        const auto& stats_row = stats_result[0];
        
        // Extract basic stats
        BlogAnalyticsDTO analytics{};
        analytics.total_users = stats_row.get<int>("total_users").value();
        analytics.active_users = stats_row.get<int>("active_users").value();
        analytics.total_posts = stats_row.get<int>("total_posts").value();
        analytics.published_posts = stats_row.get<int>("published_posts").value();
        analytics.total_comments = stats_row.get<int>("total_comments").value();
        analytics.approved_comments = stats_row.get<int>("approved_comments").value();
        analytics.total_views = stats_row.get<int>("total_views").value();
        
        // Calculate derived metrics
        analytics.avg_posts_per_user = analytics.active_users > 0 ? 
            static_cast<double>(analytics.published_posts) / analytics.active_users : 0.0;
        
        analytics.avg_comments_per_post = analytics.published_posts > 0 ?
            static_cast<double>(analytics.approved_comments) / analytics.published_posts : 0.0;
        
        analytics.approval_rate = analytics.total_comments > 0 ?
            static_cast<double>(analytics.approved_comments) / analytics.total_comments * 100.0 : 0.0;
        
        analytics.avg_views_per_post = analytics.published_posts > 0 ?
            static_cast<double>(analytics.total_views) / analytics.published_posts : 0.0;
        
        // Get most popular post
        auto popular_post_query = relx::select(p.title)
            .from(p)
            .where(p.published == true)
            .order_by(p.views.desc())
            .limit(1);
        
        auto popular_result = conn.execute(popular_post_query);
        if (popular_result && !popular_result->empty()) {
            analytics.most_popular_post = (*popular_result)[0]
                .get<std::string>("title").value_or("Unknown");
        }
        
        // Get most active user
        auto active_user_query = relx::select_expr(
            u.name
        ).from(u)
         .join(p, relx::on(u.id == p.user_id))
         .where(p.published == true)
         .group_by(u.id, u.name)
         .order_by(relx::count(p.id).desc())
         .limit(1);
        
        auto active_result = conn.execute(active_user_query);
        if (active_result && !active_result->empty()) {
            analytics.most_active_user = (*active_result)[0]
                .get<std::string>("name").value_or("Unknown");
        }
        
        return analytics;
        
    } catch (const relx::RelxException& e) {
        return std::unexpected(QueryError{
            .message = e.what(),
            .error_code = 0,
            .query_string = "blog_analytics"
        });
    }
}
```

### Content Recommendation System

Finding related posts based on tags and user behavior:

```cpp
struct PostRecommendationDTO {
    int post_id;
    std::string title;
    std::string author_name;
    int views;
    std::vector<std::string> tags;
    double relevance_score;
};

relx::ConnectionResult<std::vector<PostRecommendationDTO>> get_post_recommendations(
    relx::PostgreSQLConnection& conn,
    int user_id,
    int limit = 10
) {
    try {
        Users u;
        Posts p, user_posts;
        Tags t;
        PostTags pt, user_pt;
        
        // Get user's tag preferences based on their posts and interactions
        auto recommendations_query = relx::select_expr(
            p.id,
            p.title,
            u.name,
            p.views,
            relx::as(
                // Calculate relevance score based on tag overlap and popularity
                relx::count(relx::distinct(pt.tag_id)) * 2 + 
                relx::log(p.views + 1),
                "relevance_score"
            )
        ).from(p)
         .join(u, relx::on(p.user_id == u.id))
         .join(pt, relx::on(p.id == pt.post_id))
         .join(t, relx::on(pt.tag_id == t.id))
         .where(
             p.published == true &&
             p.user_id != user_id &&  // Don't recommend user's own posts
             pt.tag_id.in(
                 relx::select(user_pt.tag_id)
                     .from(user_posts)
                     .join(user_pt, relx::on(user_posts.id == user_pt.post_id))
                     .where(user_posts.user_id == user_id)
             )
         )
         .group_by(p.id, p.title, u.name, p.views)
         .order_by(
             relx::count(relx::distinct(pt.tag_id)).desc(),
             p.views.desc()
         )
         .limit(limit);
        
        auto recommendations = relx::value_or_throw(
            conn.execute<PostRecommendationDTO>(recommendations_query)
        );
        
        // Get tags for each recommended post
        for (auto& rec : recommendations) {
            auto tags_query = relx::select(t.name)
                .from(t)
                .join(pt, relx::on(t.id == pt.tag_id))
                .where(pt.post_id == rec.post_id)
                .order_by(t.name);
            
            auto tags_result = conn.execute(tags_query);
            if (tags_result) {
                for (const auto& tag_row : *tags_result) {
                    auto tag_name = tag_row.get<std::string>("name");
                    if (tag_name) {
                        rec.tags.push_back(*tag_name);
                    }
                }
            }
        }
        
        return recommendations;
        
    } catch (const relx::RelxException& e) {
        return std::unexpected(QueryError{
            .message = e.what(),
            .error_code = 0,
            .query_string = "post_recommendations"
        });
    }
}
```

These examples demonstrate relx's capability to handle complex, real-world database scenarios with type safety and clear, maintainable code. The library's fluent API makes even complex queries readable while providing compile-time validation and automatic result mapping.