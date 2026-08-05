# TODO

- ~~**Land ANY(array) binds.**~~ Done 2026-08-05: landed with the classic-DSL removal;
  full suite (incl. `InAnyArrayBind` integration test) green.

- **AliasedColumn by value.** Replace the `shared_ptr<Expr>` storage with a by-value
  `Expr` member (constexpr ctors + `to_sql`) so runtime-aliased columns can
  constant-evaluate in `static_sql` contexts. The pointer exists to dodge copying, not
  for shared ownership; expression objects are small. Check the callers of the
  `shared_ptr` constructor overload before deleting it. Note: even by-value, a
  runtime-string alias keeps the query out of `has_static_shape_v` (the alias text is
  part of the SQL) — the NTTP `relx::as<"name">(expr)` form remains the static-shaped
  spelling.
