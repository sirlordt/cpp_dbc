# Git Commands Reference

This file contains useful Git commands for working with the C++ Demo Application project.

## Viewing Staged Changes

To view changes that have been staged but not yet committed:

```bash
git --no-pager diff --staged
```

This command shows the differences between the staging area and the last commit. The `--no-pager` option ensures the output is displayed directly in the terminal without using a pager program like `less`.

### When to Use

- Before committing, to review what changes will be included in the commit
- To verify that the correct changes have been staged
- To create detailed commit messages based on the actual changes

### Example Output

```diff
diff --git a/src/main.cpp b/src/main.cpp
index a123456..b789012 100644
--- a/src/main.cpp
+++ b/src/main.cpp
@@ -10,7 +10,7 @@ int main(int argc, char* argv[]) {
     std::cout << "Hello, World!" << std::endl;
     
     if (argc > 1) {
-        std::cout << "Arguments: " << argc - 1 << std::endl;
+        std::cout << "Number of arguments: " << argc - 1 << std::endl;
     }
     
     return 0;
```

## Viewing Staged Changes for Documentation Updates

```bash
git --no-pager diff --cached
```

This command is identical to `git --no-pager diff --staged` (they are aliases). It shows the differences between the staging area and the last commit without using a pager program.

### When to Use

- When updating CHANGELOG.md to document recent changes
- When crafting comprehensive git commit messages
- To get a clear summary of all staged changes for documentation purposes

### Example Workflow

1. Make changes to your code
2. Stage the changes with `git add`
3. Run `git --no-pager diff --cached` to review all staged changes
4. Use the output to:
   - Update CHANGELOG.md with detailed descriptions of changes
   - Write a comprehensive commit message that accurately reflects all changes
   - Document the changes in project documentation

This approach ensures that your documentation and commit messages precisely match the actual code changes, making the project history more useful and maintainable.

## Other Useful Git Commands

### Viewing Commit History

```bash
git log
```

### Viewing Changes in Working Directory

```bash
git diff
```

### Viewing Changes for a Specific File

```bash
git diff -- path/to/file
```

### Viewing Changes Between Commits

```bash
git diff commit1..commit2
```

### Viewing Changes in Recent Commits

```bash
git --no-pager diff HEAD~1 HEAD
```

This command shows the changes between the current commit (HEAD) and the previous commit (HEAD~1). This is particularly useful for:
- Reviewing recent code quality improvements
- Verifying warning flag implementations
- Checking refactoring changes like adding `m_` prefix to member variables
- Confirming changes to build scripts and CMake files

### Viewing Changes for Documentation Updates

When updating documentation based on code changes, it's useful to see all staged changes at once:

```bash
git --no-pager diff --staged
```

This command is particularly helpful when:
- Updating CHANGELOG.md with recent changes
- Updating memory-bank files with new features or improvements
- Ensuring documentation accurately reflects code changes
- Documenting new features like BLOB support across multiple files
- Tracking license header additions to source files
- Verifying format changes in documentation files
- Documenting code quality improvements and warning flags

### Analyzing Code Quality Changes

To specifically analyze changes related to code quality improvements:

```bash
git --no-pager diff HEAD~1 HEAD | grep -E "(-W|-Werror|static_cast|m_[A-Za-z])"
```

This command filters the diff output to focus on:
- Warning flag additions (`-W` and `-Werror`)
- Type conversion improvements (`static_cast`)
- Member variable renaming with `m_` prefix

This is useful when documenting code quality improvements in:
- CHANGELOG.md
- README.md
- Technical documentation
- Memory bank files

## Git Integration with VSCode

Note that Git integration in VSCode is disabled in the container environment to prevent credential issues and unwanted Git operations. The following settings are applied:

```json
{
  "git.enabled": false,
  "git.useCredentialStore": false,
  "git.autofetch": false,
  "git.confirmSync": false
}
```

For Git operations, use the command line interface directly.

## VSCode Configuration Files

The project now includes VSCode configuration files in the `.vscode` directory:

- `c_cpp_properties.json`: Configures C/C++ IntelliSense with proper include paths and preprocessor definitions
- `tasks.json`: Defines build tasks for the project
- `build_with_props.sh`: Script to extract preprocessor definitions from `c_cpp_properties.json` and pass them to build.sh

These files help ensure consistent development environment across different machines and provide convenient build tasks directly from VSCode.

### Using VSCode Tasks

To build the project using VSCode tasks:

1. Press `Ctrl+Shift+B` to run the default build task
2. Select "CMake: build" to build using the default configuration
3. Select "Build with C++ Properties" to build using the preprocessor definitions from `c_cpp_properties.json`

The "Auto install extensions" task runs automatically when opening the folder and installs recommended extensions.
