# RapidXML 1.13 — temporary, replaced in stage 7

RapidXML 1.13, from
<https://downloads.sourceforge.net/project/rapidxml/rapidxml/rapidxml%201.13/rapidxml-1.13.zip>
(sha256 `c3f0b886374981bb20fabcf323d755db4be6dba42064599481da64a85f5b3571`),
reduced to the two headers `le/utility/xml.hpp` actually includes plus the
licence. Dual licensed Boost Software Licence 1.0 / MIT — see `license.txt`.

Vendored rather than fetched because upstream is a 2009 SourceForge zip with no
git history and no maintainer, so a submodule would point at somebody's mirror.

Preset (de)serialisation reaches this through `LE::Utility::XML`, which wraps
RapidXML closely enough that it inherits from `rapidxml::xml_node<>`. **Stage 8
replaces it with tinyxml2 via `sst-plugininfra`, keeping the on-disk schema
byte for byte** — see `doc/tech/implementation_sequence.md` §8.1. Nothing else
in the tree may include these headers directly.
