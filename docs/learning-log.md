## Day X — LLM Cache Control Plane

Learned:
- cache miss lifecycle
- routing decision
- metadata registration

Key insight:
LLM serving is really a state management problem.


Partial prefix match
Client request
      ↓
Router
      ↓
Check exact cache
      ↓
If none → check longest prefix
      ↓
Reuse partial cache
      ↓
Continue inference

