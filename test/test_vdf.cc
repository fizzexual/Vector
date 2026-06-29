#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include "core/vdf.h"

using namespace vector_core;

TEST_CASE("parses a Steam appmanifest .acf") {
  const char* acf = R"acf(
"AppState"
{
	"appid"		"1091500"
	"name"		"Cyberpunk 2077"
	"installdir"		"Cyberpunk 2077"
	"StateFlags"		"4"
	"SizeOnDisk"		"70000000000"
	"LastPlayed"		"1700000000"
	"UserConfig"
	{
		"language"		"english"
	}
}
)acf";
  auto root = parse_vdf(acf);
  REQUIRE(root.has_value());
  const VdfNode* app = root->get("AppState");
  REQUIRE(app != nullptr);
  CHECK(app->get_str("appid") == "1091500");
  CHECK(app->get_str("name") == "Cyberpunk 2077");
  CHECK(app->get_str("installdir") == "Cyberpunk 2077");
  CHECK(app->get_str("SizeOnDisk") == "70000000000");
  const VdfNode* uc = app->get("UserConfig");
  REQUIRE(uc != nullptr);
  CHECK(uc->get_str("language") == "english");
}

TEST_CASE("parses libraryfolders.vdf with escaped Windows paths") {
  const char* vdf = R"vdf(
"libraryfolders"
{
	"0"
	{
		"path"		"C:\\Program Files (x86)\\Steam"
		"label"		""
	}
	"1"
	{
		"path"		"D:\\SteamLibrary"
		"apps"
		{
			"1091500"		"12345"
		}
	}
}
)vdf";
  auto root = parse_vdf(vdf);
  REQUIRE(root.has_value());
  const VdfNode* lf = root->get("libraryfolders");
  REQUIRE(lf != nullptr);

  const VdfNode* l0 = lf->get("0");
  REQUIRE(l0 != nullptr);
  CHECK(l0->get_str("path") == "C:\\Program Files (x86)\\Steam");

  const VdfNode* l1 = lf->get("1");
  REQUIRE(l1 != nullptr);
  CHECK(l1->get_str("path") == "D:\\SteamLibrary");
  REQUIRE(l1->get("apps") != nullptr);
  CHECK(l1->get("apps")->get_str("1091500") == "12345");
}

TEST_CASE("keys are case-insensitive and defaults work") {
  auto root = parse_vdf("\"Root\" { \"Foo\" \"bar\" }");
  REQUIRE(root.has_value());
  const VdfNode* r = root->get("root");
  REQUIRE(r != nullptr);
  CHECK(r->get_str("FOO") == "bar");
  CHECK(r->get_str("missing", "fallback") == "fallback");
}

TEST_CASE("tolerates comments and conditional tokens") {
  const char* vdf = R"vdf(
"AppState"
{
	// a comment
	"appid"		"42"  [$WINDOWS]
	"name"		"Test"
}
)vdf";
  auto root = parse_vdf(vdf);
  REQUIRE(root.has_value());
  const VdfNode* app = root->get("AppState");
  REQUIRE(app != nullptr);
  CHECK(app->get_str("appid") == "42");
  CHECK(app->get_str("name") == "Test");
}
