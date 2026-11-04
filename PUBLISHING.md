# Publishing DevonCraft

This archive is a source snapshot for the DevonCraft repository. It contains the
application source, tests, documentation, generated art, the nGL source dependency,
and a preserved upstream-history bundle. The bundle is historical data; its commit
objects and embedded messages are intentionally not rewritten.

## Update an existing GitHub repository safely

1. Clone `https://github.com/dlcasto2/DevonCraft.git` and create a working branch.
2. Copy the contents of this source folder into that checkout, leaving `.git/` intact.
3. Review `git status` and `git diff`; keep the original `license.txt`, nGL license,
   and upstream credits unchanged.
4. Run the host tests and, when the Ndless r2020 toolchain is available, both
   calculator builds described in `docs/building.md`.
5. Commit the reviewed changes and push the branch. Use a pull request or an ordinary
   fast-forward update; do not force-push over existing repository history.

The `devoncraft-survival-foundation.bundle` filename is the renamed copy of the
historical bundle. Its contents are preserved byte-for-byte so the original history
and attribution remain verifiable.
