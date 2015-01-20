FILE(REMOVE_RECURSE
  "libpilight.pdb"
  "libpilight.so"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/pilight_shared.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
