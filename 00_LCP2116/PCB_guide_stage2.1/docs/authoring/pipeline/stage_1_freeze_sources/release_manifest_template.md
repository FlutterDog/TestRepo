# RELEASE MANIFEST TEMPLATE

## Document role

ℹ️ Этот файл фиксирует freeze-комплект Source of Truth для одной платы и одной ревизии.

---

## Board identity

### Board ID

* `<board_id>`

### Revision

* `<revision>`

### Freeze date

* `<yyyy-mm-dd>`

### Freeze owner

* `<name | team | none>`

---

## Source bundle

### Primary incoming bundle

* `<archive name | handoff package | none>`

### Source bundle type

* `<zip | mixed handoff | other>`

### Entry point used for freeze

* `/README.md`
* `docs/authoring/pipeline/PIPELINE.md`

### Active stage

* `Stage 1 — Freeze Sources`

---

## Included groups

### `sources/cad/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/schematic/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/pcb/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/bom/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/gerber/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/stackup/`

* Status: `<present | missing | partial>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

### `sources/extras/`

* Status: `<present | missing | partial | not used>`
* Main files:
  * `<file>`
  * `<file>`
* Notes:
  * `<notes | none>`

---

## Excluded groups and files

### Excluded from freeze

* `<file or group>` — `<reason>`
* `<file or group>` — `<reason>`
* `none`

### Conflicting candidates not included

* `<file>` — `<why excluded>`
* `<file>` — `<why excluded>`
* `none`

### Storage rule for excluded candidates

* excluded candidates are not kept inside `sources/*`
* original external handoff may still contain them outside the canonical repo freeze

---

## Known gaps

### Missing source groups

* `<group>` — `<impact>`
* `<group>` — `<impact>`
* `none`

### Known quality limits of the bundle

* `<limit>`
* `<limit>`
* `none`

---

## Naming and normalization

### External renames accepted in freeze

* `<old name>` → `<new name>` — `<why>`
* `<old name>` → `<new name>` — `<why>`
* `none`

### Kept as original on purpose

* `<file>` — `<why original name preserved>`
* `<file>` — `<why original name preserved>`
* `none`

---

## Freeze decision

### Decision

* `<accepted | accepted with gaps | blocked>`

### Why this decision

* `<reason>`
* `<reason>`

### What changes require manifest update

* change in `sources/*`
* replacement of a canonical file
* addition of a previously missing source group
* revision-mixing cleanup that changes the accepted SoT set

---

## Notes

### Freeze notes

* `<notes | none>`
