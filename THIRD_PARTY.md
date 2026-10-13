# Third-party components and distribution decisions

This source tree can use the following external components. No third-party
source is copied into this package.

| Component | Purpose | Typical licence model | Default behavior |
| --- | --- | --- | --- |
| Qt 6 Widgets / Concurrent / Network | Desktop UI, image I/O, concurrency, HTTPS API transport | Commercial or open-source (GPL/LGPL depending on module/version) | Required; installed separately |
| LibRaw | Camera RAW decoding | LGPL/CDDL dual licensing; verify selected release | Optional; discovered at build time |
| OpenAI Image API | Optional prompt-based Generative Fill service | OpenAI service terms and usage pricing | Opt-in at runtime; not linked or bundled |

Before distributing ChromaDesk:

1. Select and document the Qt commercial or open-source licence route.
2. If using an LGPL route, verify dynamic-linking, notice, source-offer,
   replacement/relinking, and installation-information obligations with counsel.
3. Preserve the licence and copyright notices shipped with Qt and LibRaw.
4. Produce an SBOM from the exact release build rather than relying only on this
   development inventory.
5. Re-check dependency versions and licences for each release.

Qt's own guidance recommends commercial licensing for non-open-source projects
and notes that dynamic linking is normally used where an LGPL application keeps
its own source proprietary. This file is an engineering checkpoint and is not
legal advice.
