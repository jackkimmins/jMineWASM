// inventory.hpp
#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include "../shared/types.hpp"
#include <array>

// Represents a slot in the hotbar
struct InventorySlot {
    BlockType blockType;
    bool isEmpty;
    
    InventorySlot() : blockType(BLOCK_STONE), isEmpty(true) {}
    InventorySlot(BlockType type) : blockType(type), isEmpty(false) {}
};

// Manages the player's hotbar inventory (9 slots)
class HotbarInventory {
private:
    static const int HOTBAR_SIZE = 9;
    std::array<InventorySlot, HOTBAR_SIZE> slots;
    int selectedSlot;
    
public:
    HotbarInventory() : selectedSlot(0) {
        // Initialize all slots as empty
        for (int i = 0; i < HOTBAR_SIZE; i++) {
            slots[i] = InventorySlot();
        }
    }
    
    // Set a block type in a specific slot
    void setSlot(int index, BlockType blockType) {
        if (index >= 0 && index < HOTBAR_SIZE) {
            slots[index] = InventorySlot(blockType);
        }
    }
    
    // Clear a slot
    void clearSlot(int index) {
        if (index >= 0 && index < HOTBAR_SIZE) {
            slots[index] = InventorySlot();
        }
    }
    
    // Get the currently selected block type
    BlockType getSelectedBlockType() const {
        if (!slots[selectedSlot].isEmpty) {
            return slots[selectedSlot].blockType;
        }
        return BLOCK_STONE; // Default fallback
    }
    
    // Get block type at a specific slot
    BlockType getBlockTypeAt(int index) const {
        if (index >= 0 && index < HOTBAR_SIZE && !slots[index].isEmpty) {
            return slots[index].blockType;
        }
        return BLOCK_STONE;
    }
    
    // Check if a slot is empty
    bool isSlotEmpty(int index) const {
        if (index >= 0 && index < HOTBAR_SIZE) {
            return slots[index].isEmpty;
        }
        return true;
    }
    
    // Get the selected slot index
    int getSelectedSlot() const {
        return selectedSlot;
    }
    
    // Select a specific slot
    void selectSlot(int index) {
        if (index >= 0 && index < HOTBAR_SIZE) {
            selectedSlot = index;
        }
    }
    
    // Scroll to next slot (wraps around)
    void scrollNext() {
        selectedSlot = (selectedSlot + 1) % HOTBAR_SIZE;
    }
    
    // Scroll to previous slot (wraps around)
    void scrollPrevious() {
        selectedSlot--;
        if (selectedSlot < 0) {
            selectedSlot = HOTBAR_SIZE - 1;
        }
    }
    
    // Get total number of slots
    int getHotbarSize() const {
        return HOTBAR_SIZE;
    }
};

#endif // INVENTORY_HPP
