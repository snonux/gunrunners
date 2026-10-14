#!/usr/bin/env bash
# Checks the web page (docs/index.html): every local file it points at
# (src, href, poster) exists, and docs/CNAME names the host of its og:url,
# so GitHub Pages serves it on the domain the page links to.
#
# Usage: tests/site/links.sh PAGE.html     check one page
#        tests/site/links.sh --self-test   prove the checks catch a broken page
set -euo pipefail

# check PAGE: prints every problem; exits non-zero if there was one.
check() {
  local page=$1 dir bad=0 ref path
  dir=$(dirname "$page")
  [[ -f "$page" ]] || { echo "missing page: $page"; return 1; }

  # Local references only: no scheme, no protocol-relative URL, no
  # in-page anchor, no data URI. A ?query or #fragment is not part of the file.
  while IFS= read -r ref; do
    case $ref in
      http:* | https:* | //* | '#'* | mailto:* | data:*) continue ;;
    esac
    path=${ref%%[?#]*}
    [[ -z "$path" ]] && continue
    if [[ ! -e "$dir/$path" ]]; then
      echo "broken reference in $page: $ref"
      bad=1
    fi
  done < <(grep -oE '(src|href|poster)="[^"]*"' "$page" | sed -E 's/^[a-z]+="(.*)"$/\1/')

  local url host
  url=$(grep -oE '<meta property="og:url" content="[^"]*"' "$page" | sed -E 's/.*content="([^"]*)"/\1/' || true)
  host=$(sed -E 's#^https?://([^/]+).*#\1#' <<< "$url")
  if [[ -z "$host" ]]; then
    echo "no og:url in $page"
    bad=1
  elif [[ "$(cat "$dir/CNAME" 2> /dev/null)" != "$host" ]]; then
    echo "$dir/CNAME does not name $host"
    bad=1
  fi
  return $bad
}

# A good page passes; a missing file, a page without og:url, a wrong CNAME
# and a missing CNAME fail.
self_test() {
  local tmp
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  mkdir -p "$tmp/media"
  touch "$tmp/media/a.jpg" "$tmp/style.css"
  echo example.org > "$tmp/CNAME"
  cat > "$tmp/index.html" << 'EOF'
<meta property="og:url" content="https://example.org/">
<link rel="stylesheet" href="style.css?v=1">
<a href="#top">top</a> <a href="https://github.com/">gh</a>
<img src="media/a.jpg"> <video poster="media/a.jpg#t=1"></video>
EOF
  check "$tmp/index.html" > /dev/null || { echo "self-test: a good page failed"; return 1; }

  sed 's#media/a.jpg">#media/missing.jpg">#' "$tmp/index.html" > "$tmp/broken.html"
  if check "$tmp/broken.html" > /dev/null; then
    echo "self-test: a missing image passed"
    return 1
  fi
  grep -v og:url "$tmp/index.html" > "$tmp/nourl.html"
  if check "$tmp/nourl.html" > /dev/null; then
    echo "self-test: a page without og:url passed"
    return 1
  fi
  echo other.org > "$tmp/CNAME"
  if check "$tmp/index.html" > /dev/null; then
    echo "self-test: a wrong CNAME passed"
    return 1
  fi
  rm "$tmp/CNAME"
  if check "$tmp/index.html" > /dev/null; then
    echo "self-test: a missing CNAME passed"
    return 1
  fi
  echo "self-test ok"
}

case ${1:-} in
  --self-test) self_test ;;
  '') echo "usage: $0 PAGE.html | --self-test" >&2; exit 2 ;;
  *) check "$1" && echo "ok: $1" ;;
esac
