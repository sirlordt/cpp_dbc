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

### Viewing Changes for Documentation Updates

When updating documentation based on code changes, it's useful to see all staged changes at once:

```bash
git --no-pager diff --staged
```

This command is particularly helpful when:
- Updating CHANGELOG.md with recent changes
- Updating memory-bank files with new features or improvements
- Ensuring documentation accurately reflects code changes

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
