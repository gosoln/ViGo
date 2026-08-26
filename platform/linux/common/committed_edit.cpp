#include "committed_edit.h"

#include "surrounding_tracker.h"

namespace nakl::linux
{
namespace
{

void DispatchCommittedEdit(CommittedTextClient &client, const EditAction &action)
{
    if (action.delete_scalars > 0)
    {
        client.DeleteBefore(action.delete_scalars);
    }
    client.Commit(action.insert);
}

} // namespace

bool ApplyCommittedEdit(CommittedTextClient &client, const EditAction &action)
{
    if (action.kind != EditAction::Kind::ReplaceBefore ||
        !client.CanReplaceBefore(action.delete_scalars, action.expected_before))
    {
        return false;
    }
    DispatchCommittedEdit(client, action);
    return true;
}

bool ApplyTrackedCommittedEdit(CommittedTextClient &client, SurroundingTracker &tracker,
                               const EditAction &action)
{
    if (action.kind != EditAction::Kind::ReplaceBefore ||
        !client.CanReplaceBefore(action.delete_scalars, action.expected_before) ||
        !tracker.ExpectReplacement(action))
    {
        return false;
    }
    DispatchCommittedEdit(client, action);
    return true;
}

} // namespace nakl::linux
