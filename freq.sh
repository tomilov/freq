LC_ALL="C" awk -F '[^A-Za-z]+' '{ for (i = 1; i <= NF; ++i) if ($i) ++w[tolower($i)] } END { for(i in w) print w[i], i }' "$1" | sort -k1gr,2 >"$2"

