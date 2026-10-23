# Ink Diary v4.1

Frozen 2026-10-23 for reMarkable 2 / OS 3.27.3.0 / Vellum-XOVI.

## Changes since v4.0

- Expanded Mermaid flowchart parsing: unquoted/inline labels, chained arrows, semicolon-separated statements and implicit references.
- Removed the obsolete top-of-screen crop; handwriting at the top is included in API input and page-change checks.
- Compact flowcharts wrap across three columns instead of forming a single tall column.
- New calendar route calculates all dates locally from month/year with Sunday- or Monday-start weeks. October 2026 correctly begins Thursday; three-line cells remain the default.
- Added INSTALL.md with installation, configuration, upgrade, removal, troubleshooting and sharing instructions.

Retains Markdown, structured checklists/tables, plotting, web search, grayscale image generation, sequence diagrams and Venn diagrams. See README.md for supported subsets and VALIDATION.md for test coverage. New visual content was tested through live API/rendering smoke tests; these tests did not insert content into a user notebook.

The included plugin is the exact installed binary, SHA-256:

    40fbb441139adbfb5875e1286251217fba33714064b4f7806ce5c139241461f3

Source, build instructions, prebuilt binaries, fonts, scripts and licenses are included. This repository edition adds Git ignore rules and repository-based transfer instructions; the tested plugin binary is unchanged. Build outputs other than the two distribution binaries, previews, personal configuration, keys, notebook data and diagnostic captures are excluded. VERSION identifies this release as 4.1; embedded legacy XOVI metadata remains 0.1.0 to preserve the tested binary. No rebuild or restart is needed merely to save this release. Previous version archives are unchanged.

Start with INSTALL.md. Verify SHA256SUMS before running install.sh. Existing API configuration is preserved by installation.
