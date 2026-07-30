# Raster store TODO

- Consider extending `sf::validate_index()` beyond rejecting `Inner` after
  additional SF invariants and their required error reporting are defined.
  This is not part of the shared-store refactor.
- Resolve the existing glTF write exception path before implementing the
  runtime codec error contract. `mesh::io::gltf::save_to_path()` currently
  throws when `cgltf_write_file()` fails, while codec operational failures are
  intended to be returned through `std::expected`. Decide whether mesh I/O
  should return a typed error or the glTF codec should catch and translate the
  exception, and explicitly confirm whether changing mesh I/O is in scope.
