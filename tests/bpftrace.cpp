#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "bpftrace.h"

namespace bpftrace {
namespace test {
namespace bpftrace {

class MockBPFtrace : public BPFtrace {
public:
  MOCK_METHOD2(find_wildcard_matches, std::set<std::string>(std::string attach_point, std::string file));
  std::vector<Probe> get_probes()
  {
    return probes_;
  }
  std::vector<Probe> get_special_probes()
  {
    return special_probes_;
  }
};

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Return;
using ::testing::StrictMock;

void check_kprobe(Probe &p, const std::string &attach_point, const std::string &prog_name)
{
  EXPECT_EQ(p.type, ProbeType::kprobe);
  EXPECT_EQ(p.attach_point, attach_point);
  EXPECT_EQ(p.prog_name, prog_name);
  EXPECT_EQ(p.name, "kprobe:" + attach_point);
}

void check_uprobe(Probe &p, const std::string &path, const std::string &attach_point, const std::string &prog_name)
{
  EXPECT_EQ(p.type, ProbeType::uprobe);
  EXPECT_EQ(p.attach_point, attach_point);
  EXPECT_EQ(p.prog_name, prog_name);
  EXPECT_EQ(p.name, "uprobe:" + path + ":" + attach_point);
}

void check_special_probe(Probe &p, const std::string &attach_point, const std::string &prog_name)
{
  EXPECT_EQ(p.type, ProbeType::uprobe);
  EXPECT_EQ(p.attach_point, attach_point);
  EXPECT_EQ(p.prog_name, prog_name);
  EXPECT_EQ(p.name, prog_name);
}

TEST(bpftrace, add_begin_probe)
{
  ast::Probe probe("BEGIN", nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 0);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 1);

  check_special_probe(bpftrace.get_special_probes().at(0), "BEGIN_trigger", "BEGIN");
}

TEST(bpftrace, add_end_probe)
{
  ast::Probe probe("END", nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 0);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 1);

  check_special_probe(bpftrace.get_special_probes().at(0), "END_trigger", "END");
}

TEST(bpftrace, add_probes_single)
{
  ast::AttachPointList attach_points = {"sys_read"};
  ast::Probe probe("kprobe", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 1);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);

  check_kprobe(bpftrace.get_probes().at(0), "sys_read", "kprobe:sys_read");
}

TEST(bpftrace, add_probes_multiple)
{
  ast::AttachPointList attach_points = {"sys_read", "sys_write"};
  ast::Probe probe("kprobe", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 2);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);

  std::string probe_prog_name = "kprobe:sys_read,sys_write";
  check_kprobe(bpftrace.get_probes().at(0), "sys_read", probe_prog_name);
  check_kprobe(bpftrace.get_probes().at(1), "sys_write", probe_prog_name);
}

TEST(bpftrace, add_probes_wildcard)
{
  ast::AttachPointList attach_points = {"sys_read", "my_*", "sys_write"};
  ast::Probe probe("kprobe", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  std::set<std::string> matches = { "my_one", "my_two" };
  ON_CALL(bpftrace, find_wildcard_matches(_, _))
    .WillByDefault(Return(matches));
  EXPECT_CALL(bpftrace,
      find_wildcard_matches("my_*",
        "/sys/kernel/debug/tracing/available_filter_functions"))
    .Times(1);

  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 4);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);

  std::string probe_prog_name = "kprobe:sys_read,my_*,sys_write";
  check_kprobe(bpftrace.get_probes().at(0), "sys_read", probe_prog_name);
  check_kprobe(bpftrace.get_probes().at(1), "my_one", probe_prog_name);
  check_kprobe(bpftrace.get_probes().at(2), "my_two", probe_prog_name);
  check_kprobe(bpftrace.get_probes().at(3), "sys_write", probe_prog_name);
}

TEST(bpftrace, add_probes_wildcard_no_matches)
{
  ast::AttachPointList attach_points = {"sys_read", "my_*", "sys_write"};
  ast::Probe probe("kprobe", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;
  std::set<std::string> matches;
  ON_CALL(bpftrace, find_wildcard_matches(_, _))
    .WillByDefault(Return(matches));
  EXPECT_CALL(bpftrace,
      find_wildcard_matches("my_*",
        "/sys/kernel/debug/tracing/available_filter_functions"))
    .Times(1);

  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 2);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);

  std::string probe_prog_name = "kprobe:sys_read,my_*,sys_write";
  check_kprobe(bpftrace.get_probes().at(0), "sys_read", probe_prog_name);
  check_kprobe(bpftrace.get_probes().at(1), "sys_write", probe_prog_name);
}

TEST(bpftrace, add_probes_uprobe)
{
  ast::AttachPointList attach_points = {"foo"};
  ast::Probe probe("uprobe", "/bin/sh", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;

  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 1);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);
  check_uprobe(bpftrace.get_probes().at(0), "/bin/sh", "foo", "uprobe:/bin/sh:foo");
}

TEST(bpftrace, add_probes_uprobe_wildcard)
{
  ast::AttachPointList attach_points = {"foo*"};
  ast::Probe probe("uprobe", "/bin/sh", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;

  EXPECT_NE(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 0);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);
}

TEST(bpftrace, add_probes_tracepoint)
{
  ast::AttachPointList attach_points = {"sched_switch"};
  ast::Probe probe("tracepoint", "sched", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;

  EXPECT_EQ(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 1);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);
}

TEST(bpftrace, add_probes_tracepoint_wildcard)
{
  ast::AttachPointList attach_points = {"sched_*"};
  ast::Probe probe("tracepoint", "sched", &attach_points, nullptr, nullptr);

  StrictMock<MockBPFtrace> bpftrace;

  EXPECT_NE(bpftrace.add_probe(probe), 0);
  EXPECT_EQ(bpftrace.get_probes().size(), 0);
  EXPECT_EQ(bpftrace.get_special_probes().size(), 0);
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> key_value_pair_int(std::vector<uint64_t> key, int val)
{
  std::pair<std::vector<uint8_t>, std::vector<uint8_t>> pair;
  pair.first  = std::vector<uint8_t>(sizeof(uint64_t)*key.size());
  pair.second = std::vector<uint8_t>(sizeof(uint64_t));

  uint8_t *key_data = pair.first.data();
  uint8_t *val_data = pair.second.data();

  for (size_t i=0; i<key.size(); i++)
  {
    *(uint64_t*)(key_data + sizeof(uint64_t)*i) = key.at(i);
  }
  *(uint64_t*)val_data = val;

  return pair;
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> key_value_pair_str(std::vector<std::string> key, int val)
{
  std::pair<std::vector<uint8_t>, std::vector<uint8_t>> pair;
  pair.first  = std::vector<uint8_t>(STRING_SIZE*key.size());
  pair.second = std::vector<uint8_t>(sizeof(uint64_t));

  uint8_t *key_data = pair.first.data();
  uint8_t *val_data = pair.second.data();

  for (size_t i=0; i<key.size(); i++)
  {
    strncpy((char*)key_data + STRING_SIZE*i, key.at(i).c_str(), STRING_SIZE);
  }
  *(uint64_t*)val_data = val;

  return pair;
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> key_value_pair_int_str(int myint, std::string mystr, int val)
{
  std::pair<std::vector<uint8_t>, std::vector<uint8_t>> pair;
  pair.first  = std::vector<uint8_t>(sizeof(uint64_t) + STRING_SIZE);
  pair.second = std::vector<uint8_t>(sizeof(uint64_t));

  uint8_t *key_data = pair.first.data();
  uint8_t *val_data = pair.second.data();

  *(uint64_t*)key_data = myint;
  strncpy((char*)key_data + sizeof(uint64_t), mystr.c_str(), STRING_SIZE);
  *(uint64_t*)val_data = val;

  return pair;
}

TEST(bpftrace, sort_by_key_int)
{
  StrictMock<MockBPFtrace> bpftrace;

  std::vector<SizedType> key_args = { SizedType(Type::integer, 8) };
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> values_by_key =
  {
    key_value_pair_int({2}, 12),
    key_value_pair_int({3}, 11),
    key_value_pair_int({1}, 10),
  };
  bpftrace.sort_by_key(key_args, values_by_key);

  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expected_values =
  {
    key_value_pair_int({1}, 10),
    key_value_pair_int({2}, 12),
    key_value_pair_int({3}, 11),
  };

  EXPECT_THAT(values_by_key, ContainerEq(expected_values));
}

TEST(bpftrace, sort_by_key_int_int)
{
  StrictMock<MockBPFtrace> bpftrace;

  std::vector<SizedType> key_args = {
    SizedType(Type::integer, 8),
    SizedType(Type::integer, 8),
    SizedType(Type::integer, 8),
  };
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> values_by_key =
  {
    key_value_pair_int({5,2,1}, 1),
    key_value_pair_int({5,3,1}, 2),
    key_value_pair_int({5,1,1}, 3),
    key_value_pair_int({2,2,2}, 4),
    key_value_pair_int({2,3,2}, 5),
    key_value_pair_int({2,1,2}, 6),
  };
  bpftrace.sort_by_key(key_args, values_by_key);

  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expected_values =
  {
    key_value_pair_int({2,1,2}, 6),
    key_value_pair_int({2,2,2}, 4),
    key_value_pair_int({2,3,2}, 5),
    key_value_pair_int({5,1,1}, 3),
    key_value_pair_int({5,2,1}, 1),
    key_value_pair_int({5,3,1}, 2),
  };

  EXPECT_THAT(values_by_key, ContainerEq(expected_values));
}

TEST(bpftrace, sort_by_key_str)
{
  StrictMock<MockBPFtrace> bpftrace;

  std::vector<SizedType> key_args = {
    SizedType(Type::string, STRING_SIZE),
  };
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> values_by_key =
  {
    key_value_pair_str({"z"}, 1),
    key_value_pair_str({"a"}, 2),
    key_value_pair_str({"x"}, 3),
    key_value_pair_str({"d"}, 4),
  };
  bpftrace.sort_by_key(key_args, values_by_key);

  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expected_values =
  {
    key_value_pair_str({"a"}, 2),
    key_value_pair_str({"d"}, 4),
    key_value_pair_str({"x"}, 3),
    key_value_pair_str({"z"}, 1),
  };

  EXPECT_THAT(values_by_key, ContainerEq(expected_values));
}

TEST(bpftrace, sort_by_key_str_str)
{
  StrictMock<MockBPFtrace> bpftrace;

  std::vector<SizedType> key_args = {
    SizedType(Type::string, STRING_SIZE),
    SizedType(Type::string, STRING_SIZE),
    SizedType(Type::string, STRING_SIZE),
  };
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> values_by_key =
  {
    key_value_pair_str({"z", "a", "l"}, 1),
    key_value_pair_str({"a", "a", "m"}, 2),
    key_value_pair_str({"z", "c", "n"}, 3),
    key_value_pair_str({"a", "c", "o"}, 4),
    key_value_pair_str({"z", "b", "p"}, 5),
    key_value_pair_str({"a", "b", "q"}, 6),
  };
  bpftrace.sort_by_key(key_args, values_by_key);

  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expected_values =
  {
    key_value_pair_str({"a", "a", "m"}, 2),
    key_value_pair_str({"a", "b", "q"}, 6),
    key_value_pair_str({"a", "c", "o"}, 4),
    key_value_pair_str({"z", "a", "l"}, 1),
    key_value_pair_str({"z", "b", "p"}, 5),
    key_value_pair_str({"z", "c", "n"}, 3),
  };

  EXPECT_THAT(values_by_key, ContainerEq(expected_values));
}

TEST(bpftrace, sort_by_key_int_str)
{
  StrictMock<MockBPFtrace> bpftrace;

  std::vector<SizedType> key_args = {
    SizedType(Type::integer, 8),
    SizedType(Type::string, STRING_SIZE),
  };
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> values_by_key =
  {
    key_value_pair_int_str(1, "b", 1),
    key_value_pair_int_str(2, "b", 2),
    key_value_pair_int_str(3, "b", 3),
    key_value_pair_int_str(1, "a", 4),
    key_value_pair_int_str(2, "a", 5),
    key_value_pair_int_str(3, "a", 6),
  };
  bpftrace.sort_by_key(key_args, values_by_key);

  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expected_values =
  {
    key_value_pair_int_str(1, "a", 4),
    key_value_pair_int_str(1, "b", 1),
    key_value_pair_int_str(2, "a", 5),
    key_value_pair_int_str(2, "b", 2),
    key_value_pair_int_str(3, "a", 6),
    key_value_pair_int_str(3, "b", 3),
  };

  EXPECT_THAT(values_by_key, ContainerEq(expected_values));
}

} // namespace bpftrace
} // namespace test
} // namespace bpftrace
