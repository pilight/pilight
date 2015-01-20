FILE(REMOVE_RECURSE
  "libpilight.pdb"
  "libpilight.a"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/pilight_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
