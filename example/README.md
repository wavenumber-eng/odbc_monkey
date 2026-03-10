# OdbcMonkey Example Library

This is a minimal working example of an OdbcMonkey-based Altium database library.

## Structure

```
example/
  example.DbLib        <- Altium database library (generated)
  symbols/
    R_2P.SchLib        <- Generic resistor symbol
    C_2P_NP.SchLib     <- Generic capacitor symbol (non-polarized)
  footprints/
    R0603_HD.PcbLib    <- 0603 resistor footprint (high density)
    C0603_HD.PcbLib    <- 0603 capacitor footprint (high density)
  json/
    EXAMPLE_RES_10K.json   <- Example 10k resistor part
    EXAMPLE_CAP_100NF.json <- Example 100nF capacitor part
```

## Rebuilding the DbLib

Run the rebuild script from the `odbc_monkey` directory:

```powershell
.\rebuild_example.ps1
```

Or use the Python module directly:

```bash
python dblib_builder.py --json-path example/json --output example --name example
```

## Using in Altium

1. **Install the odbc-monkey driver** (see `install.ps1` in parent directory)

2. **Open the DbLib** in Altium Designer:
   - File -> Open -> Select `example.DbLib`

3. **Configure library search paths** in Altium:
   - Tools -> Options -> Data Management -> Database Libraries
   - Add the `example` folder to the Symbol/Footprint search paths

4. **Place components**:
   - Open the Components panel
   - Select the example.DbLib database
   - Browse tables: `capacitors#0603`, `resistors#0603`
   - Place parts into your schematic

## JSON Format

Each JSON file represents one component with version history:

```json
{
  "versions": [
    {
      "id": "01940000-0000-7000-0000-000000000001",
      "classification": "resistors/0603",
      "cad-reference": "EXAMPLE_RES_10K",
      "Library Path": "symbols\\R_2P.SchLib",
      "Library Ref": "R_2P",
      "Footprint Path 1": "footprints\\R0603_HD.PcbLib",
      "Footprint Ref 1": "R0603_HD",
      "manufacturer": "Example Manufacturer",
      "mpn": "RC0603FR-0710KL",
      "value": "10k",
      "description": "RES SMD 10K OHM 1% 1/10W 0603"
    }
  ]
}
```

Key fields:
- `id`: UUIDv7 for version selection (optional, but recommended)
- `classification`: Determines ODBC table name (`category/table` -> `category#table`)
- `cad-reference`: Primary key for Altium lookup
- `Library Path` / `Library Ref`: Symbol reference
- `Footprint Path N` / `Footprint Ref N`: Footprint references (N = 1, 2, 3...)

See `JSON_FORMAT.md` in the parent directory for complete format documentation.

## Notes

- The `example.DbLib` contains an absolute path to the JSON directory
- For portable libraries, consider using environment variables or relative paths
- The symbol/footprint paths in JSON are relative to Altium's library search path
