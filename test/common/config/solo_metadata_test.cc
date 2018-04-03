#include "common/config/metadata.h"
#include "common/config/solo_metadata.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Config {
namespace {

TEST(SoloMetadataTest, NonEmptyStringValue) {
  envoy::api::v2::core::Metadata metadata;
  Metadata::mutableMetadataValue(metadata, "filter1", "key1")
      .set_string_value("");
  Metadata::mutableMetadataValue(metadata, "filter2", "key2")
      .set_string_value("non-empty");

  const auto filter1 = metadata.filter_metadata().find("filter1")->second;
  const auto filter2 = metadata.filter_metadata().find("filter2")->second;

  EXPECT_FALSE(SoloMetadata::nonEmptyStringValue(filter1, "key1").has_value());
  EXPECT_FALSE(SoloMetadata::nonEmptyStringValue(filter1, "key2").has_value());
  EXPECT_FALSE(SoloMetadata::nonEmptyStringValue(filter2, "key1").has_value());
  EXPECT_TRUE(SoloMetadata::nonEmptyStringValue(filter2, "key2").has_value());
  EXPECT_EQ("non-empty",
            *SoloMetadata::nonEmptyStringValue(filter2, "key2").value());
}

TEST(SoloMetadataTest, BoolValue) {
  envoy::api::v2::core::Metadata metadata;
  Metadata::mutableMetadataValue(metadata, "filter1", "key1")
      .set_bool_value(true);
  Metadata::mutableMetadataValue(metadata, "filter2", "key2")
      .set_bool_value(false);

  const auto filter1 = metadata.filter_metadata().find("filter1")->second;
  const auto filter2 = metadata.filter_metadata().find("filter2")->second;

  EXPECT_TRUE(SoloMetadata::boolValue(filter1, "key1"));
  EXPECT_FALSE(SoloMetadata::boolValue(filter1, "key2"));
  EXPECT_FALSE(SoloMetadata::boolValue(filter2, "key1"));
  EXPECT_FALSE(SoloMetadata::boolValue(filter2, "key2"));
}

} // namespace
} // namespace Config
} // namespace Envoy
