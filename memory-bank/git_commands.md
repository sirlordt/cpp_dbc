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
