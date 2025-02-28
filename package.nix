{
  lib,
  stdenv,
  cmake,
  catch2_3,
  sqlite,
  boost,
  scas,
}:

stdenv.mkDerivation {
  pname = "srdp";
  version = "0.2.0";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [ cmake ];
  buildInputs = [ sqlite boost scas ];
  nativeCheckInputs = [ catch2_3 ];

  doCheck = true;

  meta = {
    description = "Simple research data pipepline";
    license = lib.licenses.gpl3Only;
    platforms = lib.platforms.linux;
  };
}

