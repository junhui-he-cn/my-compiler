# Generic Collection Callback Inference Plan

**Goal:** Allow existing array higher-order helpers to specialize generic
callbacks from known call-site types.

**Architecture:** Reuse `inferTypeArguments` and
`substituteTypeParameters` in a shared TypeChecker helper. Generic callback
specialization is erased before IR lowering, so no bytecode or Rust VM changes
are expected.

## Tasks

- [x] Add generic callback success and unresolved-inference fixtures.
- [x] Implement callback specialization and generic-lambda contextual checking.
- [x] Apply specialization to all existing array callback helpers.
- [x] Update current language docs and generic callback design notes.
- [x] Run focused and full verification; prepare the type-system slice for
      commit.
