#pragma once

// Item types the player can hold/equip.
// Player::Item is an alias of this (shared with PickupItem).
// Values are saved as numbers (scenes, prefabs, item order), so they must not be renumbered.
// 1 used to be Thermometer (removed - the game no longer uses it).
enum class ItemType
{
    Incense = 2,
    Amulet = 3,
    FlashLight = 4
};
