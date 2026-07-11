# Projects and Sources

A WimForge project is a normal directory containing declarative configuration and local Git history. Selecting a source or customization changes project intent; it does not mount or service an image until a plan has been reviewed and confirmed.

## Create, open, and import

Use **New project** to choose a project name and directory. WimForge creates `project.json`, initializes the project repository, and creates the action-history journal under `.wimforge`.

The project sheet accepts three forms:

- an existing project directory containing `project.json`;
- a portable JSON configuration, imported into a destination directory; or
- a complete `.wimforge` bundle, validated and restored into a destination directory.

JSON is configuration interchange only. A [Project Bundle](Project-Bundles) carries complete project and notification repositories, including their Git objects and undo history.

Successful output-affecting changes use the canonical project save path: write `project.json`, create a project commit, then append the corresponding contextual action. If the secondary action-history append fails, the already-safe project commit is retained and WimForge raises a warning.

## Supported source forms

The **Source & editions** page accepts:

- an ISO file;
- an extracted Windows media directory;
- a WIM or ESD image; or
- the first part of a split SWM set.

The **Inspect** action inventories a directly readable WIM/ESD/SWM or the `sources\install.*` image in extracted media. For a raw ISO, mount or extract it first when you need immediate index inspection, then point **Image path** at the contained WIM, ESD, or first SWM. The servicing plan can still treat the ISO itself as the source to clone into a working media tree.

## Source, image, mount, and output paths

These fields have different responsibilities:

| Field | Meaning |
| --- | --- |
| **Source path** | The original ISO, media directory, or image supplied by the operator |
| **Image path** | The WIM/ESD/SWM that DISM inspection and servicing address |
| **Mount path** | An empty directory used for an offline mount |
| **Output path** | The final WIM, ESD, SWM, or ISO destination |

Keep **Clone source before editing** enabled. Offline planning then creates project-owned image/media workspaces rather than using the original as the default write target. ISO and media sources are cloned even when low-level in-place behavior is requested.

Validation rejects paths that overlap source, image, mount, working media, or output boundaries. Do not work around that check with junctions or reparse points; trust-boundary staging rejects them where traversal could escape the expected root.

## Select an edition

After inspection, choose the target edition from the discovered names or enter its one-based image index. The summary comes from image metadata returned by inspection. Edition names and available indexes vary by source; do not assume an index copied from another ISO still identifies the same edition.

## Choose an output

The desktop offers WIM, ESD, SWM, and ISO output. ISO creation also needs the Windows ADK Deployment Tools program `oscdimg`. The volume-label field is limited to 32 characters in the desktop.

WimForge builds output into project-owned work paths and promotes validated final files rather than presenting a partial file as complete. Output planning, split-image behavior, media staging, and ISO ordering are described in [Image Servicing](Image-Servicing).

## Before customization

Before adding payloads:

1. Record the source origin and SHA-256.
2. Inspect the exact image and architecture.
3. Keep source, mount, scratch, and output on storage with sufficient free space.
4. Preserve a pristine source outside the project work tree.
5. Decide whether the final artifact is an image or complete bootable media.

Then continue to [Customize](Customize). Before execution, review [Safety and Recovery](Safety-and-Recovery).

---

[← Application Tour](Application-Tour) · [Customize →](Customize)
