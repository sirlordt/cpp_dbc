# TODO

## Pending Tasks

- Add library dw to linker en CPP_SBC
- Add script for build inside a docker the creation of .deb (ubuntu 22.04) .rpm (fedora) and make simple build for another distro version
- NEW FEATURE: Add more examples.
- NEW FEATURE: Add more debug messages?
- PLANNED: Start to using in real proyect and test how ease is integrate in third party project. Maybe write a INTERGRATION.md to explain how full integrate in a real project.
- PLANNED: More db drivers
   - Firebird/Interbase
   - SQLServer/SysBase?
   - Oracle?
   - DB2?
   - Clickhouse?
   - Cassandra/Symtilla?
   - Mongo? crazy bro is not sql db!!!

## Completed Tasks

- Finish to add the license header to .cpp files
- BUG: When no pass the --auto the script is not stop before continue
- NEW FEATURE: Capture the output when the run test to .log file in the helper.sh
- NEW FEATURE: Capture the param --check-test-log in the helper.sh
- NEW FEATURE: Add specific testing of json field types.
- NEW FEATURE: Create a generic function fo capture the callstack java like
- NEW FEATURE: Extend DBException to make more robust add Mark and not concatenated to message
- NEW FEATURE: Extend DBException to make more robust add Call stack where throw the exception
