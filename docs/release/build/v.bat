mameui -validate 
xmllint --noout -valid hash/*.xml 2>&1
xmllint --noout -valid hash/more/*.xml 2>&1
