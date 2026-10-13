# Security Policy

## Reporting a vulnerability

Do not disclose security vulnerabilities in a public issue. Contact the repository
owner privately through the verified contact route on the owner's GitHub profile.
Include the affected version, platform, reproduction steps, and potential impact.

Never include API keys, private photographs, signing certificates, catalog files,
or other sensitive data in a report.

## Secrets and image privacy

ChromaDesk performs ordinary editing and local masks on the device. Generative Fill
is opt-in and sends only the bounded working image, mask, and prompt after explicit
consent. OpenAI API keys are kept in memory for the current session and must never
be committed to this repository.
