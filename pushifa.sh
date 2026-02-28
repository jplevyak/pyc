#!/bin/bash

# ==========================================
# Configuration: Edit these variables
# ==========================================
PREFIX="ifa"          # The relative path to your library
REMOTE_NAME="ifa-remote" # A local alias for the remote
REMOTE_URL="git@github.com:jplevyak/ifa.git"
TARGET_BRANCH="main"                # The branch on the standalone repo to push to
# ==========================================

# 1. Ensure we are running this from the root of a git repository
if [ ! -d ".git" ]; then
  echo "❌ Error: Please run this script from the root of your main Git repository."
  exit 1
fi

# 2. Ensure the subtree directory actually exists
if [ ! -d "$PREFIX" ]; then
  echo "❌ Error: The subtree directory '$PREFIX' does not exist."
  exit 1
fi

# 3. Check for uncommitted changes
# Pushing a subtree with a dirty working directory can lead to unpredictable results
if ! git diff-index --quiet HEAD --; then
  echo "❌ Error: You have uncommitted changes. Please commit or stash them before pushing the subtree."
  exit 1
fi

# 4. Check if the remote exists, add it if it doesn't
if ! git remote | grep -q "^$REMOTE_NAME$"; then
  echo "🌐 Remote '$REMOTE_NAME' not found. Adding it now..."
  git remote add "$REMOTE_NAME" "$REMOTE_URL"
else
  # Optional: Ensure the URL is correct if the remote already exists
  git remote set-url "$REMOTE_NAME" "$REMOTE_URL"
fi

# 5. Execute the subtree push
echo "🚀 Calculating subtree history for '$PREFIX'..."
echo "Pushing to '$REMOTE_NAME' on branch '$TARGET_BRANCH'..."

# The actual Git subtree command
if git subtree push --prefix="$PREFIX" "$REMOTE_NAME" "$TARGET_BRANCH"; then
  echo "✅ Successfully pushed subtree '$PREFIX' to upstream!"
else
  echo "❌ Error: Git subtree push failed. See output above for details."
  # A common failure reason is that the upstream has new commits you haven't pulled yet.
  echo "💡 Tip: You may need to run 'git subtree pull --prefix=$PREFIX $REMOTE_NAME $TARGET_BRANCH' first."
  exit 1
fi
