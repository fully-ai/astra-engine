# Astra Engine STRIDE Threat Model

| Asset | Threat | Mitigation |
| :--- | :--- | :--- |
| glTF / `.glb` / TGA / WAV / JSON Importers | Buffer over-read or integer overflow on crafted assets | Strict length-bounds checks (`pos + len <= buf.size()`), recursion depth cap (`<= 64`) in JSON parser, stride & index range verification |
| AstraScript `.as` Runtime | Script infinite loops freezing frame loop | Explicit instruction/step budget (`max_ops_per_call = 10000`) per frame preventing infinite loops from hanging editor |
| Project Build Pipeline | Path traversal writing packaged assets outside `dist/` | Strict path sanitization and canonicalized asset bundle manifest |
