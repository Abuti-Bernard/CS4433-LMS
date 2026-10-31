# Phase 6 — Configuration Management and Quality Assurance

This document records the configuration-management and quality-assurance process for the School LMS.

## 1. Configuration management

### Repository and branch strategy

The project is maintained in GitHub in `Abuti-Bernard/CS4433-LMS`.

- `main` is the stable integration branch. Work intended for submission should be buildable and tested before it is merged.
- New work is developed on a descriptive feature branch, for example `feature/phase-6-configuration-qa`.
- A feature branch is pushed to GitHub and merged into `main` through a pull request (PR), rather than committing routine feature work directly to `main`.
- The PR description records the purpose of the change and the checks performed. The author reviews the changed files and the CI result before merging.
- Small, focused commits use the imperative form, such as `Add LMS CI workflow` or `Document Phase 6 quality checks`.

### Release tags

Use annotated semantic-version tags for stable milestones:

- `v1.0.0` — first assessed LMS release.
- Increment the patch number for backwards-compatible fixes (for example, `v1.0.1`).
- Increment the minor number for backwards-compatible functionality (for example, `v1.1.0`).

Create a release tag on the tested `main` commit after the Phase 6 PR has been merged. Do not tag an untested feature branch.

### Continuous integration

The workflow is stored at `.github/workflows/ci.yml`. It runs on every push to a branch and on pull requests targeting `main`. The workflow:

1. Installs CMake, a C++ compiler, SQLite, OpenSSL, Python and Cppcheck.
2. Configures the C++17 project in `lms/`.
3. Builds the LMS using CMake.
4. Runs the existing end-to-end test through CTest. The test exercises the real console application and SQLite database across the administrator, lecturer, student and Head of Department roles.
5. Runs Cppcheck over `lms/src`; warnings in the enabled warning, performance and portability categories fail the job.
6. Runs Lizard to report source lines of code (NLOC) and cyclomatic complexity (CCN), then uploads the report as the `lms-quality-metrics` workflow artifact.

The project already enables compiler warnings (`-Wall -Wextra` for GCC/Clang and `/W4` for MSVC) in `lms/CMakeLists.txt`.

## 2. Quality assurance

### Static analysis

Cppcheck is the static-analysis tool. Run it from the repository root:

```bash
cppcheck --enable=warning,performance,portability \
  --error-exitcode=1 --inline-suppr \
  --suppress=missingIncludeSystem lms/src
```

The CI job uses the same command. A finding must be reviewed in context and corrected where it indicates a real defect. A suppression should only be added for a documented false positive; it must not be used to hide a genuine warning. The CI check is configured to fail when an enabled finding remains, so the check is repeatable rather than a one-time manual claim.

### Code metrics

Lizard is used to calculate NLOC and cyclomatic complexity for the C++ source files. Run:

```bash
python -m pip install lizard
lizard lms/src -l cpp
```

The output includes per-function NLOC and CCN, plus project-level totals/averages. Use the report attached to the GitHub Actions run (`lms-quality-metrics`) as the measured result for the submitted revision. CCN is the number of independent control-flow paths estimated by Lizard; a higher value suggests a function may be harder to test and maintain.

Key functions to pay attention to in the report include authentication, assignment creation, submission, marking/moderation, and the console login/menu flow. Record the measured NLOC and CCN for the selected functions from the generated report rather than estimating them by hand.

### Self-code-review checklist

Before merging a change, review the diff and check:

- [ ] The change is limited to the intended feature and does not include build output, database files, passwords or other generated/private data.
- [ ] Names and control flow are understandable; duplicated logic is avoided where practical.
- [ ] Inputs are validated and errors are handled without terminating the application unexpectedly.
- [ ] SQL uses bound parameters and database resources are closed safely.
- [ ] Role and department/course access restrictions are still enforced.
- [ ] Passwords remain salted and hashed; plaintext passwords are not written to source control or logs.
- [ ] Existing submissions, marks, feedback and audit history are not accidentally overwritten.
- [ ] The project builds cleanly and the end-to-end test passes.
- [ ] Cppcheck has no unresolved enabled findings, or any suppression is justified in a code comment.
- [ ] The CI workflow and metrics artifact correspond to the commit being reviewed.

### Review record

For this Phase 6 change, the review record should be completed from the actual GitHub Actions run:

| Check | Evidence / result |
|---|---|
| CMake build | GitHub Actions job: **Build LMS** |
| End-to-end tests | GitHub Actions job: **Run automated end-to-end tests** |
| Static analysis | GitHub Actions job: **Run Cppcheck static analysis** |
| Lines of code and CCN | Download the `lms-quality-metrics` artifact |
| Manual review | Complete the checklist above against the PR diff |

Do not report a check as passed until the corresponding workflow step has completed successfully.
