// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <gtest/gtest.h>

#include "openzl/zl_errors.h"

#ifdef ZSTRONG_COMMON_ERRORS_INTERNAL_H
#    error "Must not include errors_internal.h"
#endif

namespace {

struct Foo {};

ZL_RESULT_DECLARE_TYPE(Foo);

constexpr Foo kFoo;

TEST(PublicErrorsTest, errorCodeToString)
{
    EXPECT_NE(ZL_ErrorCode_toString(ZL_ErrorCode_GENERIC), nullptr);
}

TEST(PublicErrorsTest, errorCreation)
{
    auto report = ZL_REPORT_ERROR(allocation, "fail! %d", 12345);
    EXPECT_TRUE(ZL_isError(report));
    report = ZL_REPORT_ERROR(allocation, "fail!");
    EXPECT_TRUE(ZL_isError(report));
    report = ZL_REPORT_ERROR(allocation);
    EXPECT_TRUE(ZL_isError(report));
}

TEST(PublicErrorsTest, requireChokeOnError)
{
    auto report = ZL_REPORT_ERROR(allocation, "fail! %d", 12345);
    EXPECT_TRUE(ZL_isError(report));
}

TEST(PublicErrorsTest, retIfs)
{
    {
        auto f = [](int path) {
            switch (path) {
                case 0:
                    ZL_RET_T_RES(Foo, ZL_RESULT_WRAP_VALUE(Foo, kFoo));
                    break;
                case 1:
                    ZL_RET_T_ERR(Foo, GENERIC, "fail! %d", 1234);
                    break;
                case 2:
                    ZL_RET_T_ERR(Foo, GENERIC, "fail!");
                    break;
                case 3:
                    ZL_RET_T_ERR(Foo, GENERIC);
                    break;
                default:
                    throw std::runtime_error("!");
            }
        };
        EXPECT_FALSE(ZL_RES_isError(f(0)));
        EXPECT_TRUE(ZL_RES_isError(f(1)));
        EXPECT_TRUE(ZL_RES_isError(f(2)));
        EXPECT_TRUE(ZL_RES_isError(f(3)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF(Foo, GENERIC, !succeed, "foo %d", 1234);
            ZL_RET_T_IF(Foo, GENERIC, !succeed, "foo");
            ZL_RET_T_IF(Foo, GENERIC, !succeed);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_NE(Foo, GENERIC, 1, 2 - (int)succeed, "foo %d", 1234);
            ZL_RET_T_IF_NE(Foo, GENERIC, 1, 2 - (int)succeed, "foo");
            ZL_RET_T_IF_NE(Foo, GENERIC, 1, 2 - (int)succeed);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_EQ(Foo, GENERIC, 1, 1 + (int)succeed, "foo %d", 1234);
            ZL_RET_T_IF_EQ(Foo, GENERIC, 1, 1 + (int)succeed, "foo");
            ZL_RET_T_IF_EQ(Foo, GENERIC, 1, 1 + (int)succeed);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_GE(
                    Foo, GENERIC, 2, 1 + (2 * (int)succeed), "foo %d", 1234);
            ZL_RET_T_IF_GE(Foo, GENERIC, 2, 1 + (2 * (int)succeed), "foo");
            ZL_RET_T_IF_GE(Foo, GENERIC, 2, 1 + (2 * (int)succeed));
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_LE(
                    Foo, GENERIC, 1 + (2 * (int)succeed), 2, "foo %d", 1234);
            ZL_RET_T_IF_LE(Foo, GENERIC, 1 + (2 * (int)succeed), 2, "foo");
            ZL_RET_T_IF_LE(Foo, GENERIC, 1 + (2 * (int)succeed), 2);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_GT(
                    Foo, GENERIC, 2, 1 + (2 * (int)succeed), "foo %d", 1234);
            ZL_RET_T_IF_GT(Foo, GENERIC, 2, 1 + (2 * (int)succeed), "foo");
            ZL_RET_T_IF_GT(Foo, GENERIC, 2, 1 + (2 * (int)succeed));
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_AND(Foo, GENERIC, true, !succeed, "foo %d", 1234);
            ZL_RET_T_IF_AND(Foo, GENERIC, true, !succeed, "foo");
            ZL_RET_T_IF_AND(Foo, GENERIC, true, !succeed);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_OR(Foo, GENERIC, false, !succeed, "foo %d", 1234);
            ZL_RET_T_IF_OR(Foo, GENERIC, false, !succeed, "foo");
            ZL_RET_T_IF_OR(Foo, GENERIC, false, !succeed);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_Report report;
            if (succeed) {
                report = ZL_returnValue(1234);
            } else {
                report = ZL_REPORT_ERROR(corruption, "foo %d", 1234);
            }
            ZL_RET_T_IF_ERR(Foo, report, "foo %d", 1234);
            ZL_RET_T_IF_ERR(Foo, report, "foo");
            ZL_RET_T_IF_ERR(Foo, report);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_NULL(
                    Foo, GENERIC, succeed ? "foo" : nullptr, "foo %d", 1234);
            ZL_RET_T_IF_NULL(Foo, GENERIC, succeed ? "foo" : nullptr, "foo");
            ZL_RET_T_IF_NULL(Foo, GENERIC, succeed ? "foo" : nullptr);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
    {
        auto f = [](bool succeed) {
            ZL_RET_T_IF_NN(
                    Foo, GENERIC, !succeed ? "foo" : nullptr, "foo %d", 1234);
            ZL_RET_T_IF_NN(Foo, GENERIC, !succeed ? "foo" : nullptr, "foo");
            ZL_RET_T_IF_NN(Foo, GENERIC, !succeed ? "foo" : nullptr);
            return ZL_RESULT_WRAP_VALUE(Foo, kFoo);
        };
        EXPECT_FALSE(ZL_RES_isError(f(true)));
        EXPECT_TRUE(ZL_RES_isError(f(false)));
    }
}

} // namespace
