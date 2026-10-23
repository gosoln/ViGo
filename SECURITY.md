# Security Policy

ViGo is an input method, so it sees what you type. Privacy and security reports are taken seriously.

## Reporting a vulnerability

Please **do not open a public issue**. Use GitHub's private reporting: open the repository's **Security** tab and choose **Report a vulnerability**.

Include the affected version, platform, and steps to reproduce. Do not include real passwords or private text.

You can expect an acknowledgement within a few days. Fixes ship in a new release, and credit is given if you want it.

## Scope

- Keystroke content leaving the device, or appearing in logs or crash reports.
- Memory-safety bugs in the Rust core or the C ABI.
- Installer, update, or signing problems that let someone run unintended code.

ViGo keeps keystroke content on-device and sends nothing over the network.
