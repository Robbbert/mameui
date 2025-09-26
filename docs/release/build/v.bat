@echo off
echo Please wait...
mameui -validate > x 2>&1
type x
xmllint --noout -valid hash/*.xml 2>&1
xmllint --noout -valid hash/more/*.xml 2>&1
