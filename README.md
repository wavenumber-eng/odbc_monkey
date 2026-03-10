
                    ▓▓▓▓▓▓▓▓▓▓
                  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓
                ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
              ▓▓▓▓░░░░░░▓▓░░░░░░▓▓▓▓
          ░░░░▓▓░░░░░░░░░░░░░░░░░░▓▓░░░░
          ░░░░▓▓░░  ██░░░░░░  ██░░▓▓░░░░
            ░░▓▓░░████░░░░░░████░░▓▓░░
              ▓▓░░░░░░░░░░░░░░░░░░▓▓
                ▓▓░░░░░░░░░░░░░░▓▓
                  ▓▓▓▓░░░░░░▓▓▓▓
                      ▓▓▓▓▓▓         ░░
                    ▓▓▓▓▓▓▓▓▓▓      ▓▓
                    ▓▓▓▓▓▓▓▓▓▓    ▓▓▓▓
                  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
                  ▓▓▓▓░░▓▓░░▓▓▓▓

====================================================================
                    odbc-monkey
====================================================================

# odbc-monkey 

A high-performance json ODBC driver sepcifically designed for use with Altium Database Library (DbLib).

Please reach out to ehughes@wavenumber.net for commercial licenses and customizations (other formats besides json)


## Features

- **Schema from JSON data** - No hardcoded table schema in the driver
- **Versioned JSON support** - Extracts latest version by UUIDv7 
- **In memory cache Cache** - All accesses to to the in-memory cache for speed
- **Live cache refresh** - File watcher for real-time updates when JSON files change
- **Non-locking reads** - Allows concurrent file editing
- **Unicode support** - Full UTF-8/UTF-16 handling
- **Classification tables** - Tables based on `foo/bar --> foo#bar` format

### Install

Run the *install.ps1* power shell script.  

It will ask for admin. 

This will register a new obbc data source. 

It udpates the registry to point to the .dll in the bin folder.

###  Test with example

run *rebuild_example.ps1*

This uses the Python tooling in `dblib_builder.py` to scan the example JSON data and regenerate `example\example.DbLib` with the correct paths for this repo checkout.

You can also run the generator directly:

```
python dblib_builder.py --json-path example\json --output example --name example
```


## Connection Strings

You can setup  your dblib manually if you are insane (use the python script!)

```
# Using driver directly
DRIVER={odbc-monkey};DataSource=C:\path\to\json\files
```

## Project Structure

- `native/` - C++ driver source, native README, build/test scripts
- `example/` - Example JSON, symbols, footprints, generated DbLib
- `dblib_builder.py` - Python DbLib generator
- `rebuild_example.ps1` - Regenerates `example\example.DbLib`
- `gui/` - Python GUI viewer

## Source

The C++ driver source, native build scripts, and native tests live in `native/`. If you need to work on the driver itself, start with `native/README.md`.

## JSON File Format

See [JSON_FORMAT.md](JSON_FORMAT.md) for the full format specification.

Highlights:

Each JSON file represents one part with versioned history:

```json
{
  "versions": [
    {
      "id": "019370ab-1234-7000-8000-000000000001",
      "cad-reference": "CAP_0402_100nF",
      "Manufacturer Part Number": "GRM155R71H104KE14D",
      "Manufacturer": "Murata",
      "description": "100nF 50V X7R 0402",
      "value": "100nF",
      "classification": "capacitors/Murata_C0402",
      ...any other fields...
    }
  ]
}
```

The driver extracts the latest version based on UUIDv7 timestamp in the `id` field.

**Field name mapping**: The driver normalizes `description` -> `Description` and `value` -> `Value` for Altium compatibility. Use lowercase in JSON; ODBC consumers see capitalized names. 
**Version selection:**
- If `id` fields are present, the version with the highest UUIDv7 timestamp is used
- If no `id` fields are present, the first entry in the `versions` array is used
- The `classification` field (`category/table` format) determines which ODBC table the part belongs to
- Filename is not used for table assignment (only for internal file tracking)

### Column Schema

There is no fixed built-in schema. Columns are discovered from the JSON data loaded for each table.

In practice, a table's exposed columns are based on the fields seen while that table is loaded. If files in the same classification have inconsistent fields, not every field is guaranteed to appear as a column.

Typical tables have 40-120+ columns depending on the data.

### Tables

- `parts` - Returns all parts from all JSON files
- `foo#bar` - Returns parts filtered by classification (for example, `SELECT * FROM [capacitors#Murata_C0402]`)

Table names use `foo#bar` format derived from the JSON `classification` field.

Example:   ic/mcu  in the classification field will show up as the tbale ic#mcu in the components panel.



## Implemenation Scope

`odbc-monkey` is built specifically for Altium Designer DbLib workflows.

It implements the subset of ODBC behavior needed by Altium and is tested primarily against Altium Designer. Other ODBC clients may work for basic browsing and queries, but broad ODBC compatibility is not a project goal.

Supported scope:

- Altium DbLib table discovery and column discovery
- `SELECT * FROM Parts`
- `SELECT * FROM [category#table]`
- Simple `WHERE` clauses using `=` and `LIKE`

Non-goals:

- Full SQL parser
- Full ODBC metadata coverage
- Compatibility with generic BI tools, ORMs, or arbitrary ODBC clients

### SQL Support

This driver supports a small SQL subset intended for Altium's access patterns:

```sql
-- All parts
SELECT * FROM Parts

-- Specific table
SELECT * FROM [capacitors#Murata_C0402]

-- With WHERE clause
SELECT * FROM Parts WHERE [Manufacturer Part Number] = 'GRM155R71H104KE14D'
```

It is not a full SQL engine. Arbitrary SQL syntax, advanced predicates, and broad client/tool compatibility are out of scope.

## Test GUI Viewer

A test python GUI for browsing the data.

run using uv (toml included)

```powershell
cd gui
uv run python odbc_viewer.py
```

## Third-Party Libraries

- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++ (MIT License)

## License

AGPLv3

## Release Notes

2026-03-01 a0 : Initial public release supporting the version JSON data model
