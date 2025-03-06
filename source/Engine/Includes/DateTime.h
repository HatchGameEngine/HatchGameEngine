#ifndef DATETIME_H
#define DATETIME_H

enum class Weekday {
	SUNDAY = 0,
	MONDAY = 1,
	TUESDAY = 2,
	WEDNESDAY = 3,
	THURSDAY = 4,
	FRIDAY = 5,
	SATURDAY = 6
};

enum class TimeOfDay {
	MORNING = 0, // Hours 5AM to 11AM. (05:00 to 11:00)
	MIDDAY = 1, // Hours 12PM to 4PM. (12:00 to 16:00)
	EVENING = 2, // Hours 5PM to 8PM.  (17:00 to 20:00)
	NIGHT = 3 // Hours 9PM to 4AM.  (21:00 to 4:00)
};

#endif
