# TODO

- ~~**Land ANY(array) binds.**~~ Done 2026-08-05: landed with the classic-DSL removal;
  full suite (incl. `InAnyArrayBind` integration test) green.

- ~~**AliasedColumn by value.**~~ Done 2026-08-05: the primary template stores a
  by-value `Expr` with constexpr ctors/`to_sql`, and a runtime-aliased column now
  constant-evaluates in `static_sql` contexts (covered by
  `StaticSqlTest.RuntimeAliasedColumnConstantEvaluates`). The `shared_ptr` constructor
  is gone. The NTTP `relx::as<"name">(expr)` form remains the static-shaped spelling.

- ~~**De-erase CaseExpr.**~~ Done 2026-08-05: `CaseExpr<ElseT, WhenThens...>` stores
  its arms by value in a tuple (no `unique_ptr<SqlExpression>`, no heap), making it
  copyable, static-shaped, and constant-evaluable — the `AliasedColumn<CaseExpr>`
  shared-ownership specialization is deleted. A CASE under an NTTP alias now
  participates in `static_sql` and type-keyed SQL memoization
  (`StaticSqlTest.CaseExpressionConstantEvaluates`,
  `StaticShapeTest.CaseExpressionIsStaticShaped`), and exposes `value_type` from its
  branches so `relx::as<"name">(case_()...)` can name a synthesized-row member.
