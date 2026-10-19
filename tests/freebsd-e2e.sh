#!/bin/sh
#
# Testes no FreeBSD: os de núcleo, modelo e tela; o de integração contra o
# bectl(8) real, num pool ZFS de rascunho em md(4) (por meio de "bectl -r",
# sem tocar nos boot environments do sistema); e a inicialização da interface
# num pseudo-terminal. Precisa de root.
#
# uso: tests/freebsd-e2e.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)

step() { echo "== $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || fail "precisa ser executado como root"

for t in test_core test_model test_view; do
	step "$t"
	"$build/$t"
done

pool=bectltuitest$$
work=$(mktemp -d /var/tmp/bectl-tui-e2e.XXXXXX)
md=

cleanup() {
	set +e
	zpool destroy -f "$pool" 2>/dev/null
	[ -n "$md" ] && mdconfig -d -u "$md" 2>/dev/null
	rm -rf "$work"
}
trap cleanup EXIT

step "ZFS"
kldstat -q -m zfs || kldload zfs

step "pool de rascunho $pool"
truncate -s 1g "$work/pool.img"
md=$(mdconfig -a -t vnode -f "$work/pool.img")
# -R: altroot, para que nada do pool seja montado sobre o sistema.
zpool create -R "$work/altroot" -O mountpoint=none "$pool" "/dev/$md"
zfs create -o mountpoint=none "$pool/ROOT"
zfs create -o mountpoint=/ -o canmount=noauto "$pool/ROOT/default"
zpool set bootfs="$pool/ROOT/default" "$pool"

cat >"$work/bectl" <<WRAPPER
#!/bin/sh
exec /sbin/bectl -r $pool/ROOT "\$@"
WRAPPER
chmod +x "$work/bectl"
"$work/bectl" list

step "test_integration"
# Em sequência: o Catch2 3 embaralha a ordem por padrão.
BECTL_TEST_CMD=$work/bectl "$build/test_integration" --order decl

# A interface lista o pool de rascunho e sai com "q", num pseudo-terminal.
# O "q" só é enviado quando o BE aparece na tela: num aarch64 emulado a
# listagem demora, e uma espera fixa não basta. script -F grava a tela à
# medida que ela é desenhada; plain() tira as sequências de escape que o FTXUI
# intercala no texto.
step "interface (pseudo-terminal)"
esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}
"$work/bectl" create -e default tela-ok
set +e
(
	for i in $(seq 1 90); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq tela-ok && break
		sleep 1
	done
	printf q
) | TERM=xterm-256color timeout 180 script -F -q "$work/screen.raw" \
	"$build/bectl-tui" --bectl="$work/bectl" --icons=ascii --lang=en_US --config="$work/config" \
	>/dev/null 2>&1
rc=$?
set -e
[ "$rc" -eq 0 ] || fail "bectl-tui terminou com status $rc"
plain "$work/screen.raw" >"$work/screen.txt"
for text in "Boot Environment" "tela-ok" "2 BE(s)"; do
	grep -aq "$text" "$work/screen.txt" || fail "a tela não mostrou: $text"
done
"$build/bectl-tui" --version

step "todos os testes passaram"
