# Reuse completed online RF regions without checking the provider

Online RF imports reuse completed indexed RF leaves from a compatible
incomplete snapshot without revalidating or downloading their source
imagery. Planning treats these leaves as finished regions and discovers only
unfinished coverage. Source identifiers and processing settings must match;
they do not establish unchanged remote content.

This preserves useful progress across long interrupted imports without
repeating network discovery for finished RF tiles. It deliberately permits
cached and newly fetched imagery to reflect different provider updates. A
fresh run without a cache refreshes all coverage. Persisting a downloaded
source pyramid or checking remote freshness for cached regions was rejected
because a few repeated source downloads are acceptable, while repeating
source work for completed RF tiles is not.
