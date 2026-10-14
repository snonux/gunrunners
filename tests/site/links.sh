#!/usr/bin/env bash
# Checks the web page (docs/index.html):
# - every local file it points at (src, href, poster, srcset, in single or
#   double quotes) exists and lies inside the page's folder, the only part
#   GitHub Pages publishes (a link to ../README.md would be dead online);
# - every url(...) in the local stylesheets it links (fonts, images) names a
#   file inside that folder, too;
# - every in-page anchor (#id) and every url(#id), in the page or its
#   stylesheets (the SVG filters and gradients), has an element with that id;
# - no src/href/poster/srcset is written in a form these checks can't read
#   (unquoted, spaced around the "=", a value or a <link> tag running over
#   several lines), so nothing slips past them;
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

# check_file ROOT BASE REF: REF (no query or fragment), relative to the
# folder BASE, is a file inside the published folder ROOT.
check_file() {
  local root base=$2 ref=$3 path target
  path=${ref%%[?#]*}
  [[ -z "$path" ]] && return 0
  root=$(realpath -m "$1")
  target=$(realpath -m "$base/$path")
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
  check_file "$dir" "$dir" "${image#"https://$host/"}"
}

# has_id PAGE ID: an element of PAGE has id="ID".
has_id() {
  grep -qE "(^|[[:space:]])id=[\"']${2}[\"']" "$1" || { echo "no element with id $2"; return 1; }
}

# stylesheets PAGE: the local stylesheets the page links, whatever the order
# of the <link>'s attributes.
stylesheets() {
  grep -oE '<link [^>]*>' "$1" | grep -E "rel=[\"']stylesheet[\"']" |
    sed -nE "s/.*href=[\"']([^\"']*)[\"'].*/\1/p" | grep -vE '^(https?:)?//' || true
}

# check_css PAGE: the url(...)s of every local stylesheet the page links,
# files relative to the stylesheet, url(#id)s against the page's ids.
check_css() {
  local page=$1 dir css ref bad=0
  dir=$(dirname "$page")
  while IFS= read -r css; do
    css=${css%%[?#]*}
    [[ -f "$dir/$css" ]] || continue # a missing stylesheet is reported by check
    while IFS= read -r ref; do
      case $ref in
        http:* | https:* | //* | data:*) ;;
        '#'*) has_id "$page" "${ref#\#}" || bad=1 ;;
        *) check_file "$dir" "$(dirname "$dir/$css")" "$ref" || bad=1 ;;
      esac
    done < <(grep -oE 'url\([^)]*\)' "$dir/$css" | sed -E "s/^url\([\"']?//; s/[\"']?\)$//")
  done < <(stylesheets "$page")
  return $bad
}

# check_unreadable PAGE: attributes refs() would not see. The name must start
# the attribute (so "?src=x" inside a quoted URL is fine).
check_unreadable() {
  local name='(^|[[:space:]<])(src|href|poster|srcset)'
  if grep -nE "$name([[:space:]]+=|=[[:space:]]+|=[^\"'[:space:]])" "$1"; then
    echo "attributes above are unquoted or spaced; quote them so they get checked"
    return 1
  fi
  if grep -nE "$name=(\"[^\"]*|'[^']*)$" "$1"; then
    echo "attribute values above run over several lines; keep each on one line"
    return 1
  fi
  if grep -nE '<link([[:space:]][^>]*)?$' "$1"; then
    echo "<link> tags above run over several lines; keep each on one line"
    return 1
  fi
}

# check_ids PAGE: every url(#id) in the page names an element there.
check_ids() {
  local page=$1 id bad=0
  while IFS= read -r id; do
    has_id "$page" "$id" || bad=1
  done < <(grep -oE 'url\(#[^)]+\)' "$page" | sed -E 's/^url\(#//; s/\)$//' | sort -u)
  return $bad
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
      '#'*) has_id "$page" "${ref#\#}" || bad=1 ;;
      *) check_file "$dir" "$dir" "$ref" || bad=1 ;;
    esac
  done < <(refs "$page")
  check_meta "$page" || bad=1
  check_css "$page" || bad=1
  check_ids "$page" || bad=1
  check_unreadable "$page" || bad=1
  return $bad
}

# expect_fail NAME FILE: the check of FILE must fail (NAME says why it should).
expect_fail() {
  if check "$2" > /dev/null; then
    echo "self-test: $1 passed"
    return 1
  fi
}

# The self-test works in a fixture folder: $tmp/site/ is the published
# folder with a good page ($good), a stylesheet, media and a CNAME. Each
# test breaks one thing (a copy of the page in $bad, or the stylesheet or
# the CNAME, put back right after) and expects the check to fail. A sed
# that doesn't apply leaves a good page, which passes, so expect_fail
# reports it; the one test that breaks two things guards its sed with a
# grep. Before each group the good page must pass again, so a group that
# left the fixture broken can't make the next group's tests pass for the
# wrong reason.

# fixture: builds the good fixture in $tmp.
fixture() {
  mkdir -p "$tmp/site/media" "$tmp/site/css/fonts"
  touch "$tmp/site/media/a.jpg" "$tmp/site/media/b.jpg" "$tmp/site/css/fonts/f.woff2" "$tmp/outside.md"
  echo '@font-face { src: url(fonts/f.woff2) format("woff2"); } body { background: url("../media/a.jpg"); }
.x { fill: url(#fray); }' > "$tmp/style.keep"
  cp "$tmp/style.keep" "$tmp/site/css/style.css"
  echo example.org > "$tmp/site/CNAME"
  cat > "$good" << 'EOF'
<meta property="og:url" content="https://example.org/">
<meta property="og:image" content="https://example.org/media/a.jpg">
<link rel="stylesheet" href="css/style.css?v=1">
<a href="#top">top</a> <a href="https://github.com/x?src=site">gh</a> <main id="top"></main>
<svg><filter id="fray"></filter><rect filter="url(#fray)"/></svg>
<img src='media/a.jpg' srcset="media/a.jpg 1x, media/b.jpg 2x"> <video poster="media/a.jpg#t=1"></video>
EOF
}

# broken NAME SED: the good page edited by SED must fail the check.
broken() {
  sed "$2" "$good" > "$bad"
  expect_fail "$1" "$bad"
}

# The page's own references.
self_test_refs() {
  broken "a missing img src" 's#src=.media/a.jpg.#src="media/missing.jpg"#' &&
    broken "a missing single-quoted src" "s#src='media/a.jpg'#src='media/missing.jpg'#" &&
    broken "a missing href with a query" 's#css/style.css?v=1#css/gone.css?v=1#' &&
    broken "a missing poster" 's#poster="media/a.jpg#poster="media/gone.jpg#' &&
    broken "a missing srcset candidate" 's#media/b.jpg 2x#media/gone.jpg 2x#' &&
    broken "a link outside the published folder" 's#href="css/style.css?v=1"#href="../outside.md"#' &&
    broken "a dead in-page anchor" 's#href="\#top"#href="\#nowhere"#' &&
    broken "a dead url(#id)" 's#url(\#fray)#url(\#nowhere)#' &&
    broken "an id only in a data-id" 's#<filter id="fray">#<filter data-id="fray">#'
}

# Attributes written so the checks could not read them.
self_test_unreadable() {
  broken "a spaced attribute" 's#<img src=#<img src = #' &&
    broken "an unquoted attribute" 's#poster="media/a.jpg\#t=1"#poster=media/a.jpg#' &&
    broken "a srcset running over two lines" \
      's#srcset="media/a.jpg 1x, media/b.jpg 2x"#srcset="media/a.jpg 1x,\n media/gone.jpg 2x"#' &&
    broken "a <link> running over two lines" 's#<link rel="stylesheet" href#<link rel="stylesheet"\n href#'
}

# css SED NAME: the stylesheet edited by SED must make the good page fail.
css() {
  local rc=0
  sed "$1" "$tmp/style.keep" > "$tmp/site/css/style.css"
  expect_fail "$2" "$good" || rc=1
  cp "$tmp/style.keep" "$tmp/site/css/style.css"
  return $rc
}

# The stylesheet's url()s.
self_test_css() {
  local rc=0
  css 's#fonts/f.woff2#fonts/gone.woff2#' "a missing font in the stylesheet" &&
    css 's#../media/a.jpg#../../outside.md#' "a stylesheet url() outside the published folder" &&
    css 's#url(\#fray)#url(\#missing)#' "a stylesheet url(#id) without its element" || return 1
  # A broken stylesheet plus a reordered <link>: the grep makes sure the
  # reorder happened, or the broken stylesheet alone would fail the page.
  sed 's#fonts/f.woff2#fonts/gone.woff2#' "$tmp/style.keep" > "$tmp/site/css/style.css"
  sed 's#<link rel="stylesheet" href="css/style.css?v=1">#<link href="css/style.css?v=1" rel="stylesheet">#' \
    "$good" > "$bad"
  if ! grep -q '<link href=' "$bad"; then
    echo "self-test: the <link> was not reordered"
    rc=1
  else
    expect_fail "a missing font behind a reordered <link>" "$bad" || rc=1
  fi
  cp "$tmp/style.keep" "$tmp/site/css/style.css"
  return $rc
}

# og:url, og:image and the CNAME.
self_test_meta() {
  broken "a page without og:url" '/og:url/d' &&
    broken "a missing og:image" 's#example.org/media/a.jpg#example.org/media/gone.jpg#' &&
    broken "an og:image on another domain" 's#content="https://example.org/media#content="https://elsewhere.org/media#' ||
    return 1
  local rc=0
  echo other.org > "$tmp/site/CNAME"
  expect_fail "a wrong CNAME" "$good" || rc=1
  rm "$tmp/site/CNAME"
  expect_fail "a missing CNAME" "$good" || rc=1
  echo example.org > "$tmp/site/CNAME"
  return $rc
}

# still_good: the fixture is back to a good page after a group.
still_good() {
  check "$good" > /dev/null || { echo "self-test: fixture not restored"; check "$good"; return 1; }
}

# A good page passes; each kind of breakage fails on its own.
self_test() {
  local tmp good bad
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  good="$tmp/site/index.html"
  bad="$tmp/site/bad.html"
  fixture
  check "$good" > /dev/null || { echo "self-test: a good page failed"; check "$good"; return 1; }
  local group
  for group in self_test_refs self_test_unreadable self_test_css self_test_meta; do
    "$group" && still_good || return 1
  done
  echo "self-test ok"
}

case ${1:-} in
  --self-test) self_test ;;
  '') echo "usage: $0 PAGE.html | --self-test" >&2; exit 2 ;;
  *) check "$1" && echo "ok: $1" ;;
esac
