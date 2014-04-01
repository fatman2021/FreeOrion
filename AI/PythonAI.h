#include "AIInterface.h"

#include <string>

class PythonAI : public AIBase {
public:
    /** \name structors */ //@{
    PythonAI();
    ~PythonAI();
    //@}

    virtual void                GenerateOrders();
    virtual void                GenerateCombatSetupOrders(const CombatData& combat_data);
    virtual void                GenerateCombatOrders(const CombatData& combat_data);
    virtual void                HandleChatMessage(int sender_id, const std::string& msg);
    virtual void                HandleDiplomaticMessage(const DiplomaticMessage& msg);
    virtual void                HandleDiplomaticStatusUpdate(const DiplomaticStatusUpdateInfo& u);
    virtual void                StartNewGame();
    virtual void                ResumeLoadedGame(const std::string& save_state_string);
    virtual const std::string&  GetSaveStateString();
};
