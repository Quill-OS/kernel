#!/bin/sh

IMG=$(cat /opt/alert.ascii)
FILE="${1}"

echo "${IMG}" > "${FILE}"
echo >> "${FILE}"
echo "Error code: ${2}" >> "${FILE}"
echo >> "${FILE}"
echo "********************" >> "${FILE}"
