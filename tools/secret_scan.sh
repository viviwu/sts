#!/bin/bash
# M3-3 key-policy gate: fail if likely secrets are committed to the repo.
# Scans tracked text files only (git ls-files), so local build dirs and
# caches never trigger it. Patterns are deliberately narrow: PEM blocks,
# well-known token prefixes and quoted password assignments.
set -euo pipefail

PATTERNS=(
  '-----BEGIN .*PRIVATE KEY-----'
  'ghp_[A-Za-z0-9]{36}'
  'github_pat_[A-Za-z0-9_]{22}'
  'AKIA[0-9A-Z]{16}'
  'xox[bap]-[A-Za-z0-9-]+'
  # NB: POSIX classes, not \s — git grep -E on Linux (glibc) accepts \s as
  # whitespace but macOS (BSD regex) reads it as a literal 's', which made
  # the local scan silently weaker than CI. Keep both platforms identical.
  'password[[:space:]]*[:=][[:space:]]*['\''"][^'\''"]+['\''"]'
  'passwd[[:space:]]*[:=][[:space:]]*['\''"][^'\''"]+['\''"]'
  # P0 gateway operators (ADR-049): quoted non-empty credentials in tracked
  # yaml are refused (private live configs stay out of the repo; fixtures
  # use empty or unquoted placeholders, which this rule ignores).
  'credential[[:space:]]*:[[:space:]]*['\''"][^'\''"]+['\''"]'
  'api[_-]?key[[:space:]]*[:=][[:space:]]*['\''"][^'\''"]+['\''"]'
  'secret[[:space:]]*[:=][[:space:]]*['\''"][^'\''"]{4,}['\''"]'
)

failures=0
for pattern in "${PATTERNS[@]}"; do
  # --include would need git grep pathspec magic; filter by extension instead.
  # NB: the script itself holds the pattern literals, so exclude it.
  matches=$(git grep -nEI --cached -e "$pattern" -- '*.cpp' '*.hpp' '*.yaml' '*.yml' '*.json' '*.md' '*.sh' ':!tools/secret_scan.sh' || true)
  if [ -n "$matches" ]; then
    echo "secret-scan: pattern '$pattern' matched:"
    echo "$matches"
    failures=1
  fi
done

if [ "$failures" -ne 0 ]; then
  echo "secret-scan FAILED: move secrets to environment variables (see qts/config/credentials.hpp)."
  exit 1
fi
echo "secret-scan ok: no likely secrets in tracked files."
