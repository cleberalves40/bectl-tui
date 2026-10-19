#!/bin/sh
#
# Teste de fumaça da interface num pseudo-terminal, com um bectl falso: ela
# precisa listar os BEs e sair com "q". Funciona com o script(1) do BSD e com
# o do util-linux. Não precisa de root nem de ZFS.
#
# uso: tests/tui-smoke.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/bectl-tui-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT

cat >"$work/bectl" <<'EOF'
#!/bin/sh
if [ "$*" = "list -H -s" ]; then
	printf 'default\n  zroot/ROOT/default\tNR\t/\t1.2G\t2026-10-01 10:00\n'
	printf '  zroot/ROOT/default@antes\t-\t-\t300M\t2026-10-04 09:00\n'
elif [ "$*" = "list -H" ]; then
	printf 'default\tNR\t/\t1.2G\t2026-10-01 10:00\n'
	printf 'tela-ok\t-\t-\t5M\t2026-10-03 12:00\n'
fi
EOF
chmod +x "$work/bectl"

esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}

cmd="$build/bectl-tui --bectl=$work/bectl --icons=ascii --lang=en_US --config=$work/config"
# O "q" só é enviado quando o BE aparece na tela (até 60 s).
keys() {
	for i in $(seq 1 60); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq tela-ok && break
		sleep 1
	done
	printf q
}
set +e
if script --version 2>/dev/null | grep -q util-linux; then
	keys | TERM=xterm-256color timeout 120 script -q -f -e -c "$cmd" "$work/screen.raw" >/dev/null 2>&1
else
	# shellcheck disable=SC2086
	keys | TERM=xterm-256color timeout 120 script -F -q "$work/screen.raw" $cmd >/dev/null 2>&1
fi
rc=$?
set -e

plain "$work/screen.raw" >"$work/screen.txt"
[ "$rc" -eq 0 ] || { cat "$work/screen.txt"; echo "FAIL: bectl-tui terminou com status $rc" >&2; exit 1; }
for text in "Boot Environment" "tela-ok" "2 BE(s)" "Snapshots (1)"; do
	grep -aq "$text" "$work/screen.txt" || { echo "FAIL: a tela não mostrou: $text" >&2; exit 1; }
done
echo "== interface ok"
