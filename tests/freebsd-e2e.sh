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

# A interface num pseudo-terminal, contra o pool de rascunho: lista, cria um
# BE pelo diálogo (Ctrl+N), destrói-o (End, Del, "y") e sai com "q". Cada
# tecla só é enviada quando a tela mostra o resultado da anterior: num aarch64
# emulado o bectl demora, e uma espera fixa não basta. script -F grava a tela
# à medida que ela é desenhada; plain() tira as sequências de escape que o
# FTXUI intercala no texto. Como root, não há doas: o bectl roda direto.
step "interface (pseudo-terminal)"
esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}
wait_for() {
	for i in $(seq 1 120); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq "$1" && return 0
		sleep 1
	done
}
"$work/bectl" create -e default tela-ok
set +e
(
	wait_for tela-ok
	printf '\016'
	wait_for "Create Boot Environment"
	printf 'via-tui\r'
	wait_for "Boot Environment 'via-tui' created."
	printf '\r'
	wait_for "3 BE(s)"
	printf '\033[F'
	sleep 1
	printf '\033[3~'
	wait_for "Confirm destroy"
	printf y
	wait_for "Destroyed (1)"
	printf '\r'
	sleep 2
	printf q
) | TERM=xterm-256color timeout 600 script -F -q "$work/screen.raw" \
	"$build/bectl-tui" --bectl="$work/bectl" --icons=ascii --lang=en_US --config="$work/config" \
	>/dev/null 2>&1
rc=$?
set -e
plain "$work/screen.raw" >"$work/screen.txt"
[ "$rc" -eq 0 ] || { tail -c 4000 "$work/screen.txt"; fail "bectl-tui terminou com status $rc"; }
for text in "Boot Environment" "tela-ok" "2 BE(s)" "3 BE(s)" "Boot Environment 'via-tui' created." \
	"Destroyed (1)"; do
	grep -aq "$text" "$work/screen.txt" || { tail -c 4000 "$work/screen.txt"; fail "a tela não mostrou: $text"; }
done
"$work/bectl" list -H | grep -q '^via-tui' && fail "via-tui não foi destruído"
"$build/bectl-tui" --version

step "todos os testes passaram"
