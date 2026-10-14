#!/usr/bin/env bash
# Checks the web page (docs/index.html):
# - every local file it points at (src, href, poster, srcset, in single or
#   double quotes) exists and lies inside the page's folder, the only part
#   GitHub Pages publishes (a link to ../README.md would be dead online);
# - every in-page anchor (#id) has an element with that id;
# - og:url and og:image are absolute URLs on the page's domain, og:image
#   names a file of the page, and docs/CNAME names that domain, so GitHub
#   Pages serves the page where it says it lives.
#
# Usage: tests/site/links.sh PAGE.html     check one page
#        tests/site/links.sh --self-test   prove the checks catch broken pages
set -euo pipefail

# refs PAGE: every src/href/poster value and every srcset candidate, one a line.
refs() {
  grep -oE "(src|href|poster|srcset)=(\"[^\"]*\"|'[^']*')" "$1" |
    sed -E "s/^([a-z]+)=[\"'](.*)[\"']$/\\1 \\2/" |
    while read -r attr value; do
      if [[ $attr == srcset ]]; then
        tr ',' '\n' <<< "$value" | awk 'NF { print $1 }'
      else
        printf '%s\n' "$value"
      fi
    done
}

# meta PAGE PROPERTY: the content of <meta property="PROPERTY">.
meta() {
  grep -oE "<meta property=\"$2\" content=\"[^\"]*\"" "$1" | sed -E 's/.*content="([^"]*)"/\1/' || true
}

# check_file DIR REF: REF (no query or fragment) is a file inside DIR.
check_file() {
  local dir=$1 ref=$2 path root target
  path=${ref%%[?#]*}
  [[ -z "$path" ]] && return 0
  root=$(realpath -m "$dir")
  target=$(realpath -m "$dir/$path")
  if [[ "$target" != "$root"/* ]]; then
    echo "reference outside the published folder: $ref"
    return 1
  fi
  [[ -e "$target" ]] || { echo "broken reference: $ref"; return 1; }
}

# check_meta PAGE: og:url and og:image against docs/CNAME.
check_meta() {
  local page=$1 dir url image host cname
  dir=$(dirname "$page")
  url=$(meta "$page" og:url)
  image=$(meta "$page" og:image)
  host=$(sed -nE 's#^https://([^/]+)/.*#\1#p' <<< "$url")
  [[ -n "$host" ]] || { echo "og:url is missing or not https://HOST/...: '$url'"; return 1; }
  cname=$(cat "$dir/CNAME" 2> /dev/null || true)
  [[ "$cname" == "$host" ]] || { echo "$dir/CNAME ('$cname') does not name $host"; return 1; }
  [[ "$image" == "https://$host/"* ]] || { echo "og:image is not on https://$host/: '$image'"; return 1; }
  check_file "$dir" "${image#"https://$host/"}"
}

# check PAGE: prints every problem; exits non-zero if there was one.
check() {
  local page=$1 dir bad=0 ref
  [[ -f "$page" ]] || { echo "missing page: $page"; return 1; }
  dir=$(dirname "$page")
  while IFS= read -r ref; do
    case $ref in
      http:* | https:* | //* | mailto:* | data:*) ;;
      '#') ;;
      '#'*) grep -qE "id=[\"']${ref#\#}[\"']" "$page" || { echo "no element with id ${ref#\#}"; bad=1; } ;;
      *) check_file "$dir" "$ref" || bad=1 ;;
    esac
  done < <(refs "$page")
  check_meta "$page" || bad=1
  return $bad
}

# expect_fail NAME FILE: the check of FILE must fail (NAME says why it should).
expect_fail() {
  if check "$2" > /dev/null; then
    echo "self-test: $1 passed"
    return 1
  fi
}

# A good page passes; each kind of breakage fails on its own.
self_test() {
  local tmp good
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  mkdir -p "$tmp/site/media"
  touch "$tmp/site/media/a.jpg" "$tmp/site/media/b.jpg" "$tmp/site/style.css" "$tmp/outside.md"
  echo example.org > "$tmp/site/CNAME"
  good="$tmp/site/index.html"
  cat > "$good" << 'EOF'
<meta property="og:url" content="https://example.org/">
<meta property="og:image" content="https://example.org/media/a.jpg">
<link rel="stylesheet" href="style.css?v=1">
<a href="#top">top</a> <a href="https://github.com/">gh</a> <main id="top"></main>
<img src='media/a.jpg' srcset="media/a.jpg 1x, media/b.jpg 2x"> <video poster="media/a.jpg#t=1"></video>
EOF
  check "$good" > /dev/null || { echo "self-test: a good page failed"; check "$good"; return 1; }

  local bad="$tmp/site/bad.html"
  sed 's#src=.media/a.jpg.#src="media/missing.jpg"#' "$good" > "$bad"
  expect_fail "a missing img src" "$bad" || return 1
  sed "s#src='media/a.jpg'#src='media/missing.jpg'#" "$good" > "$bad"
  grep -q missing "$bad" && expect_fail "a missing single-quoted src" "$bad" || return 1
  sed 's#style.css?v=1#gone.css?v=1#' "$good" > "$bad"
  expect_fail "a missing href with a query" "$bad" || return 1
  sed 's#poster="media/a.jpg#poster="media/gone.jpg#' "$good" > "$bad"
  expect_fail "a missing poster" "$bad" || return 1
  sed 's#media/b.jpg 2x#media/gone.jpg 2x#' "$good" > "$bad"
  expect_fail "a missing srcset candidate" "$bad" || return 1
  sed 's#href="style.css?v=1"#href="../outside.md"#' "$good" > "$bad"
  expect_fail "a link outside the published folder" "$bad" || return 1
  sed 's#href="\#top"#href="\#nowhere"#' "$good" > "$bad"
  expect_fail "a dead in-page anchor" "$bad" || return 1
  grep -v og:url "$good" > "$bad"
  expect_fail "a page without og:url" "$bad" || return 1
  sed 's#example.org/media/a.jpg#example.org/media/gone.jpg#' "$good" > "$bad"
  expect_fail "a missing og:image" "$bad" || return 1
  sed 's#content="https://example.org/media#content="https://elsewhere.org/media#' "$good" > "$bad"
  expect_fail "an og:image on another domain" "$bad" || return 1
  echo other.org > "$tmp/site/CNAME"
  expect_fail "a wrong CNAME" "$good" || return 1
  rm "$tmp/site/CNAME"
  expect_fail "a missing CNAME" "$good" || return 1
  echo "self-test ok"
}

case ${1:-} in
  --self-test) self_test ;;
  '') echo "usage: $0 PAGE.html | --self-test" >&2; exit 2 ;;
  *) check "$1" && echo "ok: $1" ;;
esac
