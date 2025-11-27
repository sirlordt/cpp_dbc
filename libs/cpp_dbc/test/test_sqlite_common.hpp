/**

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file test_sqlite_common.hpp
 @brief Tests for SQLite database operations

*/

#pragma once

#include <string>
#include <memory>
#include <iostream>

#include <cpp_dbc/cpp_dbc.hpp>

#include "test_main.hpp"

#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#include <cpp_dbc/drivers/sqlite_blob.hpp>
#endif

namespace sqlite_test_helpers
{

    // Empty namespace

} // namespace sqlite_test_helpers