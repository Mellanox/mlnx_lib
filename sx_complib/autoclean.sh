rm -rf aclocal.m4 autom4te.cache stamp-h1 libtool configure config.* Makefile.in Makefile
rm -rf config/*

for a in complib sx_log sx_rpc; do
	rm -rf $a/Makefile 2>&1
	rm -rf $a/Makefile.in 2>&1
done
