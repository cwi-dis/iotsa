#!/bin/bash
case x$3 in
x)
	echo Usage: $0 hostname path-to-CAcertificate path-to-CAkey
	exit 1
esac
HOSTNAME="$1"
CSR="tls.$HOSTNAME.csr"
CSRCONFIG="tls.$HOSTNAME.csrconfig"
CRT="$2"
KEY="$3"
OUT="tls.$HOSTNAME.crt"

openssl x509 -req -in $CSR -CA $CRT -CAkey $KEY -CAcreateserial -out $OUT -days 1825 -sha256 -extfile $CSRCONFIG
