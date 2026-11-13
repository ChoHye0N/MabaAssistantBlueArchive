# Schedule

How the schedule task spends its tickets (`tasks/schedule.json`, step `schedule_rooms`). Game facts are in `game-structure.md`.

## What a lesson gives
- One ticket per lesson. Tickets refill at the daily reset; the maximum grows with the sum of the location ranks (3 at first, 7 from a sum of 80). More can be bought with pyroxene, which this program never does.
- Relationship points go to **owned** students in the room only, so a room with more owned students gives more for the same ticket.
- Every lesson gives its location 100 experience. A higher location rank raises the reward chances.
- When the students of a room change during the day is not known (**verify**).

## Screens
- Location list: twelve locations by name. Inside a location: the room map, the location's name and rank at the top right, and the 'all schedules' button, which lists the rooms with their students.
- An arrow at the right edge of a location moves to the next location in list order, and from the last one back to the first (seen 2026-11-13 with all twelve open). The list is not needed to move on.
- In the room list an owned student has a pink portrait frame, a student not owned a grey one. A room done today shows a green check on its owned students.
- A relationship rank-up can show a moment after a lesson and takes the next click.

## How the task works
1. From the location list, enter the first chosen location (found by name with OCR). This is the only time the list is scrolled.
2. With "rooms with more owned students first": go round all locations once with the arrow. Chosen locations get their room list opened and every room that is not done is noted with its number of owned students; other locations are passed. The location is told by its name panel (OCR).
3. Sort the rooms by owned students (ties keep list order) and keep as many as there are tickets.
4. Go round once more and run the kept rooms where they are. Nothing is looked up again: a kept room that does not open is skipped and its ticket stays.
5. Without the setting: one lap, the rooms of the chosen locations in list order until the tickets are gone.

Popups are dismissed before each arrow press, and a press after which the location did not change is repeated once. The plan is kept by location name, so a missed press cannot shift it.

## Ideas seen elsewhere, not taken
Another automation tool scores every room with weights for the reward row, the hearts, locked rooms and students the user marked with a portrait image, and uses the same arrow to walk the locations. Here the score is the number of owned students only. Preferring particular students needs portrait templates and is left for later.
