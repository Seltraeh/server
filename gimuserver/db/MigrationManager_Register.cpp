#include "MigrationManager.hpp"
#include "migrations/CreateDefaultTables.hpp"
#include "migrations/CreateUserUnitsTable.hpp"
#include "migrations/AddTutorialEndFlag.hpp"
#include "migrations/CreateDailyTaskTables.hpp"
#include "migrations/CreateUserItemsTable.hpp"

#define ADD(x) m_migs.push_back(std::make_shared<Migrations::##x>());

void MigrationManager::Register()
{
    ADD(CreateDefaultTables);
    ADD(CreateUserUnitsTable);
    ADD(AddTutorialEndFlag);
    ADD(CreateDailyTaskTables);
    ADD(CreateUserItemsTable);
}