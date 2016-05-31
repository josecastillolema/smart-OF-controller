echo Uso: ./)
set -v
sudo mn --topo=linear,$1 --controller=remote
