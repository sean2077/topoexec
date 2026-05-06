# GitHub Pages Documentation Site

## Status

TopoExec publishes a documentation-site workflow definition for GitHub Pages.
The workflow builds the Markdown docs with MkDocs and copies the optional
Doxygen HTML reference under `/api/`. Do not add a public Pages URL to the root
README until the workflow has deployed successfully for the repository.

## Local build

Install docs-only tools in your preferred isolated environment, then run:

```bash
python -m pip install mkdocs
./scripts/docs_build_site.sh
```

The script configures a docs-only CMake build with `TOPOEXEC_BUILD_DOCS=ON`,
runs `cmake --build build-docs --target topoexec_doxygen`, builds the Markdown
site with `mkdocs build --strict`, and copies Doxygen output into `site/api/`.
Normal runtime builds do not require MkDocs or Doxygen.

## CI build

The Pages workflow lives at `.github/workflows/pages.yml` and uses GitHub Pages
Actions deployment:

1. install C++ and docs-only dependencies;
2. configure `build-docs` with `TOPOEXEC_BUILD_DOCS=ON`;
3. build the `topoexec_doxygen` target;
4. build the MkDocs site in strict mode;
5. copy `build-docs/docs/doxygen/html/` to `site/api/`;
6. upload and deploy the Pages artifact.

Repository settings must use **GitHub Actions** as the Pages source. Until a
human enables that setting and the workflow succeeds, the deployed URL is
intentionally undocumented.

## Validation

Use these checks before changing docs-site wiring:

```bash
./scripts/goal_check.sh docs
./scripts/docs_build_site.sh
```

`docs_command_smoke` remains the canonical executable Markdown command smoke.
The Pages build is a publication check; it must not replace runtime build/test
validation.
