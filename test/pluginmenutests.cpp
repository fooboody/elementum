// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/test/unit_test.hpp>

#include "../src/ui/contextmenus.hpp"

using element::PluginsPopupMenu;

BOOST_AUTO_TEST_SUITE (PluginMenuTests)

BOOST_AUTO_TEST_CASE (ManufacturerInitialIndexesAreClassified)
{
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("Acme Audio"), "A");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("zplane"), "Z");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("  8Dio"), "0-9");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex (""), "Other");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("  "), "Other");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("!Factory"), "Other");
    BOOST_CHECK_EQUAL (PluginsPopupMenu::getManufacturerInitialIndex ("株式会社"), "Other");
}

BOOST_AUTO_TEST_SUITE_END()
