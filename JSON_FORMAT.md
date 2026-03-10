# odbc-monkey JSON Format Specification

This document describes the JSON format expected by the odbc-monkey ODBC driver.

## Overview

odbc-monkey is an ODBC driver that presents JSON files as SQL tables to Altium Designer. It reads a directory of JSON files and exposes them as queryable database tables through the ODBC interface.

## JSON Format

odbc-monkey requires a `versions` array wrapper. Everything else is optional with sensible defaults.

### Minimal Valid Format

The simplest valid JSON file:

```json
{
  "versions": [
    {"cad-reference": "PART1", "value": "100nF"}
  ]
}
```

This will:
- Load into the `orphanage` table (no classification)
- Use the first/only version (no id needed for single version)

### Recommended Format

For production use, include `id` and `classification`:

```json
{
  "versions": [
    {
      "id": "0199fe35-26d0-706f-6bb6-5d53ea8396d3",
      "classification": "capacitors/Murata_C0402",
      "cad-reference": "CAP_100nF_0402",
      "mpn": "GRM155R71H104KE14D",
      "manufacturer": "Murata",
      "value": "100nF",
      "description": "CAP CER 100NF 50V X7R 0402"
    }
  ]
}
```

### Fields

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `versions` | **Yes** | - | Array wrapper (at least one object required) |
| `id` | No | First in array wins | UUIDv7 for version selection |
| `classification` | No | `orphanage` | Table routing (`category/table` format) |
| (other fields) | No | - | Passed through as ODBC columns |

### UUIDv7 Version Selection

When a JSON file contains multiple versions, odbc-monkey selects the **latest** based on UUIDv7 timestamp:

- UUIDv7 encodes a timestamp in the first 48 bits (12 hex characters)
- The version with the highest timestamp is served to Altium
- Other versions are preserved in the file but not exposed via ODBC

```json
{
  "versions": [
    {"id": "0199fe35-0000-7000-0000-000000000001", ...},  // older
    {"id": "0199fe35-ffff-7000-0000-000000000002", ...}   // newer (served)
  ]
}
```

### Missing or Invalid UUIDv7

The `id` field is optional. If missing or invalid, the version still loads:

| Scenario | Behavior |
|----------|----------|
| Valid UUIDv7 `id` | Timestamp extracted, highest wins |
| Missing `id` field | Timestamp = 0 |
| Invalid `id` format | Timestamp = 0 |
| All versions have no `id` | First version in array is used |

When multiple versions have the same timestamp (including 0), the **first one encountered** is used.

```json
{
  "versions": [
    {"classification": "caps/0402", "cad-reference": "CAP1", ...},  // No id - this one used
    {"classification": "caps/0402", "cad-reference": "CAP1", ...}   // No id - ignored
  ]
}
```

**Recommendation**: Always include valid UUIDv7 `id` fields for predictable version selection.

### Classification Field

The `classification` field determines which ODBC table the part belongs to:

| Classification Value | ODBC Table Name |
|---------------------|-----------------|
| `"capacitors/Murata_C0402"` | `capacitors#Murata_C0402` |
| `"resistors/0603"` | `resistors#0603` |
| `"connectors/USB"` | `connectors#USB` |
| `"resistors"` (no `/`) | `resistors` |
| `""` (empty string) | `orphanage` |
| `"   "` (whitespace only) | `orphanage` |
| Field missing entirely | `orphanage` |

The `/` separator is converted to `#` because Altium cannot handle `/` in table names.

The `orphanage` table acts as a catch-all for unclassified parts. Parts in orphanage are still accessible via Altium but indicate they need proper classification.

## Unsupported Formats

The following JSON formats are **NOT supported** and will be silently skipped:

```json
// NOT SUPPORTED: Simple array
[
  {"cad-reference": "PART1", ...},
  {"cad-reference": "PART2", ...}
]

// NOT SUPPORTED: Simple object (no versions wrapper)
{
  "cad-reference": "PART1",
  "mpn": "ABC123",
  ...
}

// NOT SUPPORTED: Empty versions array
{
  "versions": []
}
```

## File Organization

### One Part Per File (Recommended)

The recommended pattern is one component per JSON file:

```
json/
  CAP_100nF_0402.json      -> contains versions of CAP_100nF_0402
  RES_10k_0603.json        -> contains versions of RES_10k_0603
  USB_TYPE_C_RECEPT.json   -> contains versions of USB_TYPE_C_RECEPT
```

### Filename Does NOT Matter

The filename is **not used** for data routing or identification:
- Table assignment comes from the `classification` field inside the JSON
- Part identity comes from `cad-reference` or other fields
- Filename is only used internally for file change tracking

You could rename all files to GUIDs and the system would still work:
```
json/
  a1b2c3d4.json   -> classification inside determines table
  e5f6g7h8.json   -> classification inside determines table
```

### Multiple Versions in One File

A single file can contain multiple versions of the same part:

```json
{
  "versions": [
    {
      "id": "0199fe35-0000-7000-0000-000000000001",
      "classification": "capacitors/Murata_C0402",
      "cad-reference": "CAP_100nF_0402",
      "value": "100nF",
      "description": "Initial version"
    },
    {
      "id": "0199fe35-ffff-7000-0000-000000000002",
      "classification": "capacitors/Murata_C0402",
      "cad-reference": "CAP_100nF_0402",
      "value": "100nF",
      "description": "Updated description with better specs"
    }
  ]
}
```

**Important**: Only the latest version (by UUIDv7 timestamp) is served via ODBC. The version history is preserved in the file for auditing but not exposed to Altium.

## Field Name Mapping

odbc-monkey maps certain canonical field names to Altium-expected names:

| JSON Field (canonical) | ODBC Column (Altium) |
|-----------------------|---------------------|
| `description` | `Description` |
| `value` | `Value` |
| (all others) | (unchanged) |

This allows the JSON to use lowercase canonical names while Altium sees the capitalized names it expects.


This allows Altium to see library changes without restarting.

## DbLib Configuration

The `.DbLib` file configures Altium to connect to odbc-monkey:

```ini
[DatabaseLinks]
ConnectionString=DRIVER={odbc-monkey};DataSource=C:\path\to\json

[Table1]
TableName=capacitors#Murata_C0402
Enabled=True
UserWhere=1
UserWhereText=[cad-reference] = '{cad-reference}'
```

Use `dblib_builder.py` to generate DbLib files automatically:

```bash
python -m odbc_monkey.dblib_builder \
    --json-path C:\libs\json \
    --output C:\libs\dblib \
    --name myproject
```

## Example

A complete working example is provided in the `example/` subdirectory:

```
example/
  example.DbLib        <- Generated database library
  symbols/             <- Schematic symbols
  footprints/          <- PCB footprints
  json/                <- Part definitions
  README.md            <- Usage instructions
```

To rebuild the example:
```powershell
.\rebuild_example.ps1
```

