#gyp-to-pri

An ugly hack to create qmake .pro/.pri files for a gyp-based project (e. g. pdfium).

Example usage, assuming you have a submodule/subfolder "pdfium" in the current directory containing the pdfium sources:

***WARNING:*** this will overwrite potentially existing .pro/.pri files.

```bash
$ ~/path/to/gyp-to-pri pdfium/pdfium.gyp
```
