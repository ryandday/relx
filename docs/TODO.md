# TODO

- ~~**Land ANY(array) binds.**~~ Done 2026-08-05: landed with the classic-DSL removal;
  full suite (incl. `InAnyArrayBind` integration test) green.

- ~~**AliasedColumn by value.**~~ Done 2026-08-05: the primary template stores a
  by-value `Expr` with constexpr ctors/`to_sql`, and a runtime-aliased column now
  constant-evaluates in `static_sql` contexts (covered by
  `StaticSqlTest.RuntimeAliasedColumnConstantEvaluates`). The `shared_ptr` constructor
  is gone; its one caller was `as(CaseExpr&&)`, which now uses an explicit
  `AliasedColumn<CaseExpr>` specialization with shared ownership (CaseExpr is move-only
  and type-erased on the heap, so it could never constant-evaluate regardless). The
  NTTP `relx::as<"name">(expr)` form remains the static-shaped spelling.
