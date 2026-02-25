# DRD Suppressions

This directory contains Valgrind DRD suppression files for the cpp_dbc project.

## Purpose

DRD (Data Race Detector) is an alternative Valgrind tool to Helgrind for detecting thread synchronization errors. Sometimes, it reports false positives or known issues in third-party libraries that you want to suppress.

## Usage

### Generate Suppressions

To run tests and generate suppression rules:

```bash
./helper.sh --run-test=drd-gs,rebuild,sqlite,postgres,mysql,auto,run=1
```

This will run DRD with `--gen-suppressions=all`, which outputs suppression rules to the console when it detects issues.

### Use Custom Suppressions

To run tests with custom suppressions from `suppression.conf`:

```bash
./helper.sh --run-test=drd-s,rebuild,sqlite,postgres,mysql,auto,run=1
```

This will use the suppressions defined in `suppression.conf` file. If the file doesn't exist, the test execution will stop with an error message.

## Creating suppression.conf

1. Run tests with `--drd-gs` to generate suppression rules
2. Copy the generated suppression blocks from the output
3. Create `suppression.conf` in this directory
4. Paste the suppression blocks into the file

Example suppression format:

```
{
   <insert_a_suppression_name_here>
   drd:ConflictingAccess
   fun:function_name
   obj:*/library.so*
}
```

## Notes

- The `suppression.conf` file is **ignored by git** to avoid committing machine-specific suppressions
- Only suppress known false positives or third-party library issues
- Document why each suppression is needed (use comments in the file)
- Review suppressions periodically to ensure they're still necessary
- DRD and Helgrind use different suppression formats - they are **not** interchangeable
