# Raster store TODO

- RF builder: reject attribution indices at or above 65535 and references
  outside the selected attribution table before producing tiles. Tile storage
  does not scan attribution rasters on each read or write.

- Consider extending `sf::validate_index()` beyond rejecting `Inner` after
  additional SF invariants and their required error reporting are defined.
  This is not part of the shared-store refactor.
