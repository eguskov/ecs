#ifdef _DEBUG
#include <gtest/gtest.h>

#include <ecs/ecs.h>
#include <ecs/hash.h>

static const size_t TEMPLATES_COUNT = 8;

const CompDesc* find_component(const char *name, const EntityTemplate &templ)
{
  for (const auto &c : templ.components)
    if (c.name == name)
      return &c;
  return nullptr;
}

#define STR(x) #x

#define CHECK_COMPONENT(n, type) \
  { \
    auto c = find_component(n, t); \
    ASSERT_TRUE(c != nullptr); \
    ASSERT_TRUE(c->desc != nullptr); \
    EXPECT_STREQ(c->desc->name, Desc<type>::name); \
    EXPECT_STREQ(c->name.str, n); \
    EXPECT_EQ(c->name.hash, hash::cstr(n).hash); \
  } \

#define CHECK_COMPONENT_VALUE(n, type, v) \
  CHECK_COMPONENT(n, type) \
  { \
    auto c = find_component(n, t); \
    type value; \
    c->desc->init((uint8_t*)&value, jvalue[n]); \
    type localValue = v; \
    GTEST_TEST_BOOLEAN_(c->desc->equal((uint8_t*)&value, (uint8_t*)&localValue), n " == " STR(v), false, true, GTEST_NONFATAL_FAILURE_); \
  } \

TEST(Template, Load)
{
  EXPECT_EQ(TEMPLATES_COUNT, g_mgr->templates.size());

  for (auto &t : g_mgr->templates)
  {
    EXPECT_TRUE(t.archetypeId >= 0);
    EXPECT_TRUE(t.docId >= 0);
  }
}

TEST(Template, Load_Values)
{
  ASSERT_GE(g_mgr->templates.size(), 1ul);

  auto &t = g_mgr->templates[0];
  EXPECT_TRUE(t.name == "test-templ-for-values");

  JFrameValue jvalue(rapidjson::kObjectType);
  JValue empltyValue(rapidjson::kObjectType);
  g_mgr->buildComponentsValuesFromTemplate(0, empltyValue, jvalue);

  ASSERT_EQ(9, jvalue.MemberCount());
  ASSERT_EQ(9, t.components.size());

  CHECK_COMPONENT_VALUE("bool-comp", bool, true);
  CHECK_COMPONENT_VALUE("float-comp", float, 666.6f);
  CHECK_COMPONENT_VALUE("int-comp", int, 28);
  CHECK_COMPONENT_VALUE("string-comp", eastl::string, "hello");
  CHECK_COMPONENT_VALUE("hash-string-comp", StringHash, "hello, world");
  CHECK_COMPONENT_VALUE("vec2-comp", glm::vec2, glm::vec2(1.2f, 3.4f));
  CHECK_COMPONENT_VALUE("vec3-comp", glm::vec3, glm::vec3(1.2f, 3.4f, 5.6f));
  CHECK_COMPONENT_VALUE("vec4-comp", glm::vec4, glm::vec4(1.2f, 3.4f, 5.6f, 7.8f));

  CHECK_COMPONENT("tag-comp", Tag);
}

TEST(Template_Extends, Single)
{
  ASSERT_GE(g_mgr->templates.size(), 2ul);

  auto &t = g_mgr->templates[1];
  EXPECT_TRUE(t.name == "test-templ-for-extends-single");

  JFrameValue jvalue(rapidjson::kObjectType);
  JValue empltyValue(rapidjson::kObjectType);
  g_mgr->buildComponentsValuesFromTemplate(1, empltyValue, jvalue);

  ASSERT_EQ(9, jvalue.MemberCount());
  ASSERT_EQ(9, t.components.size());

  CHECK_COMPONENT_VALUE("bool-comp", bool, true);
  CHECK_COMPONENT_VALUE("int-comp", int, 28);
  CHECK_COMPONENT_VALUE("string-comp", eastl::string, "hello");
  CHECK_COMPONENT_VALUE("hash-string-comp", StringHash, "hello, world");
  CHECK_COMPONENT_VALUE("vec3-comp", glm::vec3, glm::vec3(1.2f, 3.4f, 5.6f));
  CHECK_COMPONENT_VALUE("vec4-comp", glm::vec4, glm::vec4(1.2f, 3.4f, 5.6f, 7.8f));

  // Overriden
  CHECK_COMPONENT_VALUE("float-comp", float, 777.8f);
  CHECK_COMPONENT_VALUE("vec2-comp", glm::vec2, glm::vec2(9.9f, 10.10f));

  CHECK_COMPONENT("tag-comp", Tag);
}

TEST(Template_Extends, Chain)
{
  ASSERT_GE(g_mgr->templates.size(), 3ul);

  auto &t = g_mgr->templates[2];
  EXPECT_TRUE(t.name == "test-templ-for-extends-chain");

  JFrameValue jvalue(rapidjson::kObjectType);
  JValue empltyValue(rapidjson::kObjectType);
  g_mgr->buildComponentsValuesFromTemplate(2, empltyValue, jvalue);

  ASSERT_EQ(9, jvalue.MemberCount());
  ASSERT_EQ(9, t.components.size());

  CHECK_COMPONENT_VALUE("bool-comp", bool, true);
  CHECK_COMPONENT_VALUE("int-comp", int, 28);
  CHECK_COMPONENT_VALUE("float-comp", float, 777.8f);
  CHECK_COMPONENT_VALUE("hash-string-comp", StringHash, "hello, world");
  CHECK_COMPONENT_VALUE("vec2-comp", glm::vec2, glm::vec2(9.9f, 10.10f));
  CHECK_COMPONENT_VALUE("vec4-comp", glm::vec4, glm::vec4(1.2f, 3.4f, 5.6f, 7.8f));

  // Overriden
  CHECK_COMPONENT_VALUE("string-comp", eastl::string, "hello, again!");
  CHECK_COMPONENT_VALUE("vec3-comp", glm::vec3, glm::vec3(7.7f, 7.7f, 9.9f));

  CHECK_COMPONENT("tag-comp", Tag);
}

TEST(Template_Extends, Multiple)
{
  ASSERT_GE(g_mgr->templates.size(), 5ul);

  auto &t = g_mgr->templates[4];
  EXPECT_TRUE(t.name == "test-templ-for-extends-multiple");

  JFrameValue jvalue(rapidjson::kObjectType);
  JValue empltyValue(rapidjson::kObjectType);
  g_mgr->buildComponentsValuesFromTemplate(4, empltyValue, jvalue);

  ASSERT_EQ(10, jvalue.MemberCount());
  ASSERT_EQ(10, t.components.size());

  CHECK_COMPONENT_VALUE("bool-comp", bool, true);
  CHECK_COMPONENT_VALUE("int-comp", int, 28);
  CHECK_COMPONENT_VALUE("float-comp", float, 777.8f);
  CHECK_COMPONENT_VALUE("hash-string-comp", StringHash, "hello, world");
  CHECK_COMPONENT_VALUE("vec2-comp", glm::vec2, glm::vec2(9.9f, 10.10f));
  CHECK_COMPONENT_VALUE("vec4-comp", glm::vec4, glm::vec4(1.2f, 3.4f, 5.6f, 7.8f));

  // Overriden
  CHECK_COMPONENT_VALUE("string-comp", eastl::string, "hello, again!");
  CHECK_COMPONENT_VALUE("vec3-comp", glm::vec3, glm::vec3(7.7f, 7.7f, 9.9f));

  CHECK_COMPONENT_VALUE("extra-comp", eastl::string, "extra!");

  CHECK_COMPONENT("tag-comp", Tag);
}

TEST(Template, Order)
{
  EXPECT_EQ(0ul, g_mgr->order.size());
}
#endif